#include "tbx/platform/window.h"
#include "tbx/debug/log.h"
#include "tbx/gfx/gpu.h"
#include "tbx/platform/input.h"
#include <SDL3/SDL.h>
#include <optional>

namespace tbx::windows
{
    // One shared GL context serves every window (SDL makes it current against any of them):
    // created with the first backend, destroyed when the last backend dies. GL-context
    // mirrors are the sanctioned mutable process statics.
    static SDL_GLContext g_gl_context = nullptr;
    static int g_backend_count = 0;

    /// @brief
    /// Purpose: SDL-side window handles; live behind the Window::Backend seam so SDL types
    /// never escape this backend folder. Applied-* fields diff against the Window's data each
    /// update. The destructor releases the OS window; the last one down takes the shared GL
    /// context and SDL itself with it.
    struct Window::Backend
    {
        Backend() = default;
        ~Backend()
        {
            if (window)
                SDL_DestroyWindow(window);
            --g_backend_count;
            if (g_backend_count == 0)
            {
                if (g_gl_context)
                {
                    SDL_GL_DestroyContext(g_gl_context);
                    g_gl_context = nullptr;
                }
                SDL_Quit();
            }
        }

        Backend(const Backend&) = delete;
        Backend& operator=(const Backend&) = delete;

        SDL_Window* window = nullptr;
        SDL_WindowID id = 0;
        std::string applied_title = {};
        bool applied_vsync = false;
        // Identity of the applied icon pixels: a re-set icon arrives as a freshly allocated
        // vector, so the data pointer changing is a cheap change check.
        const void* applied_icon = nullptr;
        CursorMode applied_cursor_mode = CursorMode::NORMAL;
        bool is_first_frame = true;
    };

    //// TRANSLATION ////

    static Key translate_key(const SDL_Scancode scancode)
    {
        // Contiguous SDL ranges map onto contiguous Key ranges.
        if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z)
            return static_cast<Key>(static_cast<int>(Key::A) + (scancode - SDL_SCANCODE_A));
        if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9)
            return static_cast<Key>(static_cast<int>(Key::NUM_1) + (scancode - SDL_SCANCODE_1));
        if (scancode == SDL_SCANCODE_0)
            return Key::NUM_0;
        if (scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12)
            return static_cast<Key>(static_cast<int>(Key::F1) + (scancode - SDL_SCANCODE_F1));

        switch (scancode)
        {
            case SDL_SCANCODE_ESCAPE:
                return Key::ESCAPE;
            case SDL_SCANCODE_TAB:
                return Key::TAB;
            case SDL_SCANCODE_CAPSLOCK:
                return Key::CAPS_LOCK;
            case SDL_SCANCODE_SPACE:
                return Key::SPACE;
            case SDL_SCANCODE_RETURN:
                return Key::ENTER;
            case SDL_SCANCODE_BACKSPACE:
                return Key::BACKSPACE;
            case SDL_SCANCODE_DELETE:
                return Key::DEL;
            case SDL_SCANCODE_INSERT:
                return Key::INSERT;
            case SDL_SCANCODE_HOME:
                return Key::HOME;
            case SDL_SCANCODE_END:
                return Key::END;
            case SDL_SCANCODE_PAGEUP:
                return Key::PAGE_UP;
            case SDL_SCANCODE_PAGEDOWN:
                return Key::PAGE_DOWN;
            case SDL_SCANCODE_LEFT:
                return Key::LEFT;
            case SDL_SCANCODE_RIGHT:
                return Key::RIGHT;
            case SDL_SCANCODE_UP:
                return Key::UP;
            case SDL_SCANCODE_DOWN:
                return Key::DOWN;
            case SDL_SCANCODE_LSHIFT:
                return Key::LEFT_SHIFT;
            case SDL_SCANCODE_RSHIFT:
                return Key::RIGHT_SHIFT;
            case SDL_SCANCODE_LCTRL:
                return Key::LEFT_CTRL;
            case SDL_SCANCODE_RCTRL:
                return Key::RIGHT_CTRL;
            case SDL_SCANCODE_LALT:
                return Key::LEFT_ALT;
            case SDL_SCANCODE_RALT:
                return Key::RIGHT_ALT;
            case SDL_SCANCODE_MINUS:
                return Key::MINUS;
            case SDL_SCANCODE_EQUALS:
                return Key::EQUALS;
            case SDL_SCANCODE_LEFTBRACKET:
                return Key::LEFT_BRACKET;
            case SDL_SCANCODE_RIGHTBRACKET:
                return Key::RIGHT_BRACKET;
            case SDL_SCANCODE_BACKSLASH:
                return Key::BACKSLASH;
            case SDL_SCANCODE_SEMICOLON:
                return Key::SEMICOLON;
            case SDL_SCANCODE_APOSTROPHE:
                return Key::APOSTROPHE;
            case SDL_SCANCODE_GRAVE:
                return Key::GRAVE;
            case SDL_SCANCODE_COMMA:
                return Key::COMMA;
            case SDL_SCANCODE_PERIOD:
                return Key::PERIOD;
            case SDL_SCANCODE_SLASH:
                return Key::SLASH;
            default:
                return Key::UNKNOWN;
        }
    }

    static MouseButton translate_mouse_button(const Uint8 button)
    {
        switch (button)
        {
            case SDL_BUTTON_LEFT:
                return MouseButton::LEFT;
            case SDL_BUTTON_RIGHT:
                return MouseButton::RIGHT;
            case SDL_BUTTON_MIDDLE:
                return MouseButton::MIDDLE;
            default:
                return MouseButton::COUNT;
        }
    }

    //// BACKEND LIFECYCLE ////

    static void open_backend(Window& window)
    {
        if (g_backend_count == 0)
        {
            if (!SDL_Init(SDL_INIT_VIDEO))
            {
                TBX_ERROR("SDL_Init failed: {}", SDL_GetError());
                std::abort();
            }
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
            SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
            SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        }

        auto backend = std::make_unique<Window::Backend>();
        ++g_backend_count; // paired with the decrement in ~Backend
        backend->window = SDL_CreateWindow(
            window.title.c_str(),
            window.width,
            window.height,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        if (!backend->window)
        {
            TBX_ERROR("SDL_CreateWindow failed: {}", SDL_GetError());
            std::abort();
        }
        backend->id = SDL_GetWindowID(backend->window);
        backend->applied_title = window.title;

        if (!g_gl_context)
        {
            g_gl_context = SDL_GL_CreateContext(backend->window);
            if (!g_gl_context)
            {
                TBX_ERROR("SDL_GL_CreateContext failed: {}", SDL_GetError());
                std::abort();
            }
            gpu::initialize();
        }
        SDL_GL_MakeCurrent(backend->window, g_gl_context);
        // The swap interval sticks per window surface, not per context.
        SDL_GL_SetSwapInterval(window.is_vsync_enabled ? 1 : 0);
        backend->applied_vsync = window.is_vsync_enabled;

        SDL_GetWindowSizeInPixels(backend->window, &window.width, &window.height);
        // Custom-pipeline hosts call gpu::begin_frame between run() calls without touching
        // the viewport; keep the gpu drawable mirror sized to the current window for them.
        gpu::set_viewport(window.width, window.height);
        window.backend = std::move(backend);
    }

    static void apply_window_data(Window& window)
    {
        Window::Backend& backend = *window.backend;
        if (window.title != backend.applied_title)
        {
            SDL_SetWindowTitle(backend.window, window.title.c_str());
            backend.applied_title = window.title;
        }
        if (window.is_vsync_enabled != backend.applied_vsync)
        {
            SDL_GL_MakeCurrent(backend.window, g_gl_context);
            SDL_GL_SetSwapInterval(window.is_vsync_enabled ? 1 : 0);
            backend.applied_vsync = window.is_vsync_enabled;
        }
        if (!window.icon_pixels.empty() && window.icon_pixels.data() != backend.applied_icon
            && window.icon_width > 0 && window.icon_height > 0
            && window.icon_pixels.size()
                   >= static_cast<size>(window.icon_width) * window.icon_height * 4)
        {
            SDL_Surface* surface = SDL_CreateSurfaceFrom(
                window.icon_width,
                window.icon_height,
                SDL_PIXELFORMAT_RGBA32,
                // SDL takes a non-const pointer; the surface only reads and is destroyed
                // below.
                const_cast<std::byte*>(window.icon_pixels.data()),
                window.icon_width * 4);
            if (surface)
            {
                SDL_SetWindowIcon(backend.window, surface);
                SDL_DestroySurface(surface);
            }
            else
                TBX_WARN("window icon surface failed: {}", SDL_GetError());
            backend.applied_icon = window.icon_pixels.data();
        }
    }

    //// EVENT ROUTING ////

    static std::optional<std::reference_wrapper<Window>> find_window(
        WindowsState& state,
        const SDL_WindowID id)
    {
        for (Window& window : state.windows)
            if (window.backend && window.backend->id == id)
                return window;
        return {};
    }

    static void close_window(Window& window)
    {
        // The OS window hides now but its backend lives until the Window itself dies: the
        // shared GL context (and every GPU cache above it) must outlive this frame.
        window.status = WindowStatus::CLOSED;
        SDL_HideWindow(window.backend->window);
    }

    //// WINDOW ////

    Window::Window() = default;
    Window::~Window() = default;
    Window::Window(Window&&) noexcept = default;
    Window& Window::operator=(Window&&) noexcept = default;

    //// WINDOWS ////

    void update(
        WindowsState& state,
        input::InputState& input,
        events::EventsState& events)
    {
        for (Window& window : state.windows)
        {
            if (window.status != WindowStatus::OPEN)
                continue;
            if (!window.backend)
                open_backend(window);
            Window::Backend& backend = *window.backend;
            // Present what was drawn since the last update; a fresh backend has nothing yet.
            if (!backend.is_first_frame)
                SDL_GL_SwapWindow(backend.window);
            backend.is_first_frame = false;
            apply_window_data(window);
        }
        if (g_backend_count == 0)
            return; // headless — no OS windows, nothing to pump

        // Gameplay asks for a cursor mode through input; the main window owns the OS cursor.
        // LOCKED = SDL relative mode: invisible, pinned to the window, movement arriving
        // purely as deltas.
        if (Window& main = state.windows.front(); main.backend)
        {
            const CursorMode cursor_mode = input.cursor_mode;
            if (cursor_mode != main.backend->applied_cursor_mode)
            {
                SDL_SetWindowRelativeMouseMode(
                    main.backend->window,
                    cursor_mode == CursorMode::LOCKED);
                if (cursor_mode == CursorMode::NORMAL)
                    SDL_ShowCursor();
                else
                    SDL_HideCursor();
                main.backend->applied_cursor_mode = cursor_mode;
            }
        }

        auto event = SDL_Event {};
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_EVENT_QUIT:
                    for (Window& window : state.windows)
                        if (window.backend && window.status == WindowStatus::OPEN)
                            close_window(window);
                    break;
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    if (const auto window = find_window(state, event.window.windowID))
                        close_window(window->get());
                    break;
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP:
                {
                    const Key key = translate_key(event.key.scancode);
                    if (key == Key::UNKNOWN)
                        break;
                    if (!event.key.repeat)
                        input::feed_key(input, key, event.key.down);
                    events.key.emit(
                        {.key = key, .is_down = event.key.down, .is_repeat = event.key.repeat != 0});
                    break;
                }
                case SDL_EVENT_MOUSE_MOTION:
                    input::feed_mouse_move(
                        input,
                        Vec2(event.motion.x, event.motion.y),
                        Vec2(event.motion.xrel, event.motion.yrel));
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP:
                {
                    const MouseButton button = translate_mouse_button(event.button.button);
                    if (button != MouseButton::COUNT)
                        input::feed_mouse_button(input, button, event.button.down);
                    break;
                }
                case SDL_EVENT_MOUSE_WHEEL:
                    input::feed_scroll(input, event.wheel.y);
                    break;
                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                    if (const auto window = find_window(state, event.window.windowID))
                    {
                        window->get().width = event.window.data1;
                        window->get().height = event.window.data2;
                        // The engine's render loop re-sizes the gpu drawable per window;
                        // the mirror tracks the main window for custom-pipeline hosts.
                        if (&window->get() == &state.windows.front())
                            gpu::set_viewport(event.window.data1, event.window.data2);
                        events.window_resized.emit(
                            {.width = event.window.data1, .height = event.window.data2});
                    }
                    break;
                default:
                    break;
            }
        }
    }

    void make_current(const Window& window)
    {
        if (window.backend)
            SDL_GL_MakeCurrent(window.backend->window, g_gl_context);
    }
}
