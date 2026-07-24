#include "tbx/assets/assets.h"
#include "platform/window_internal.h"
#include "gfx/gpu_internal.h"
#include "tbx/debug/log.h"
#include "tbx/gfx/gpu.h"
#include "tbx/platform/input.h"
#include "tbx/platform/window.h"
#include <SDL3/SDL.h>
#include <array>
#include <optional>

namespace tbx
{
    namespace internal
    {
    // One shared GL context serves every window (SDL makes it current against any of them):
    // created with the first backend, destroyed when the last backend dies. GL-context
    // mirrors are the sanctioned mutable process statics.
    static SDL_GLContext g_gl_context = nullptr;
    static int g_backend_count = 0;
    }

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
            --internal::g_backend_count;
            if (internal::g_backend_count == 0)
            {
                if (internal::g_gl_context)
                {
                    SDL_GL_DestroyContext(internal::g_gl_context);
                    internal::g_gl_context = nullptr;
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
        // The icon asset last applied — re-apply only when the handle points at a different asset.
        Uuid applied_icon_id = {};
        CursorMode applied_cursor_mode = CursorMode::NORMAL;
        WindowMode applied_mode = WindowMode::WINDOWED;
        bool is_first_frame = true;
    };

    //// BACKEND LIFECYCLE ////

    namespace internal
    {
    static void open_backend(Window& window)
    {
        if (internal::g_backend_count == 0)
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
        ++internal::g_backend_count; // paired with the decrement in ~Backend
        // The window's mode folds into the creation flags — cheaper and flicker-free vs. creating a
        // plain window and toggling it right after.
        SDL_WindowFlags mode_flags = 0;
        if (window.mode == WindowMode::FULLSCREEN)
            mode_flags = SDL_WINDOW_FULLSCREEN;
        else if (window.mode == WindowMode::BORDERLESS)
            mode_flags = SDL_WINDOW_BORDERLESS;
        backend->window = SDL_CreateWindow(
            window.title.c_str(),
            static_cast<int>(window.width),
            static_cast<int>(window.height),
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | mode_flags);
        if (!backend->window)
        {
            TBX_ERROR("SDL_CreateWindow failed: {}", SDL_GetError());
            std::abort();
        }
        backend->id = SDL_GetWindowID(backend->window);
        backend->applied_title = window.title;
        backend->applied_mode = window.mode;

        if (!internal::g_gl_context)
        {
            internal::g_gl_context = SDL_GL_CreateContext(backend->window);
            if (!internal::g_gl_context)
            {
                TBX_ERROR("SDL_GL_CreateContext failed: {}", SDL_GetError());
                std::abort();
            }
            internal::initialize_rendering();
        }
        SDL_GL_MakeCurrent(backend->window, internal::g_gl_context);
        // The swap interval sticks per window surface, not per context.
        SDL_GL_SetSwapInterval(is_vsync_enabled() ? 1 : 0);
        backend->applied_vsync = is_vsync_enabled();

        int pixel_width = 0;
        int pixel_height = 0;
        SDL_GetWindowSizeInPixels(backend->window, &pixel_width, &pixel_height);
        window.width = static_cast<uint32>(pixel_width);
        window.height = static_cast<uint32>(pixel_height);
        // Custom-pipeline hosts call begin_render_frame between run() calls without touching
        // the viewport; keep the gpu drawable mirror sized to the current window for them.
        set_render_viewport(pixel_width, pixel_height);
        window.backend = std::move(backend);
    }

    static void apply_window_data(Window& window, AssetsState& assets, EventsState& events)
    {
        Window::Backend& backend = *window.backend;
        if (window.title != backend.applied_title)
        {
            SDL_SetWindowTitle(backend.window, window.title.c_str());
            backend.applied_title = window.title;
        }
        if (is_vsync_enabled() != backend.applied_vsync)
        {
            SDL_GL_MakeCurrent(backend.window, internal::g_gl_context);
            SDL_GL_SetSwapInterval(is_vsync_enabled() ? 1 : 0);
            backend.applied_vsync = is_vsync_enabled();
        }
        if (window.mode != backend.applied_mode)
        {
            SDL_SetWindowFullscreen(backend.window, window.mode == WindowMode::FULLSCREEN);
            SDL_SetWindowBordered(backend.window, window.mode != WindowMode::BORDERLESS);
            backend.applied_mode = window.mode;
        }
        // The icon is a Texture asset: load it (once, when the handle changes) and read its pixels
        // here, rather than the runtime carrying a decoded pixel buffer on the window.
        if (window.icon.is_set() && window.icon.id != backend.applied_icon_id)
        {
            if (const auto loaded = load_now(assets, events, window.icon))
            {
                const Texture& icon = loaded->get();
                if (icon.width > 0 && icon.height > 0
                    && icon.pixels.size() >= static_cast<size>(icon.width) * icon.height * 4)
                {
                    SDL_Surface* surface = SDL_CreateSurfaceFrom(
                        icon.width,
                        icon.height,
                        SDL_PIXELFORMAT_RGBA32,
                        // SDL takes a non-const pointer; the surface only reads and is destroyed below.
                        const_cast<std::byte*>(icon.pixels.data()),
                        icon.width * 4);
                    if (surface)
                    {
                        SDL_SetWindowIcon(backend.window, surface);
                        SDL_DestroySurface(surface);
                    }
                    else
                        TBX_WARN("window icon surface failed: {}", SDL_GetError());
                }
                backend.applied_icon_id = window.icon.id;
            }
            else
                TBX_WARN("window icon: {}", loaded.error());
        }
    }
    }

    //// EVENT ROUTING ////

    namespace internal
    {
    static std::optional<std::reference_wrapper<Window>> find_window(
        WindowsState& state,
        const SDL_WindowID id)
    {
        for (Window& window : state.open_windows)
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
    }

    //// WINDOW ////

    Window::Window() = default;
    Window::~Window() = default;
    Window::Window(Window&&) noexcept = default;
    Window& Window::operator=(Window&&) noexcept = default;

    //// WINDOWS ////

    void internal::update_windows(
        WindowsState& state,
        InputState& input,
        EventsState& events,
        AssetsState& assets)
    {
        for (Window& window : state.open_windows)
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
            apply_window_data(window, assets, events);
        }
        if (internal::g_backend_count == 0)
            return; // headless — no OS windows, nothing to pump

        // Gameplay asks for a cursor mode through input; the main window owns the OS cursor.
        // LOCKED = SDL relative mode: invisible, pinned to the window, movement arriving
        // purely as deltas.
        if (Window& main = state.open_windows.front(); main.backend)
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

        // update_windows runs before update_input each frame and drains ONLY the window/quit
        // band [SDL_EVENT_QUIT, SDL_EVENT_WINDOW_LAST] here — pumping the OS queue, taking the
        // events it owns, and leaving keyboard/mouse/controller events for update_input's
        // catch-all drain. (A single-poll drain here would consume input events before input
        // ever saw them.) Handled types below act; the rest of the band is discarded.
        SDL_PumpEvents();
        auto events_buffer = std::array<SDL_Event, 64> {};
        int count = 0;
        while ((count = SDL_PeepEvents(
                    events_buffer.data(),
                    static_cast<int>(events_buffer.size()),
                    SDL_GETEVENT,
                    SDL_EVENT_QUIT,
                    SDL_EVENT_WINDOW_LAST))
               > 0)
        {
            for (int i = 0; i < count; ++i)
            {
                const SDL_Event& event = events_buffer[static_cast<size>(i)];
                switch (event.type)
                {
                    case SDL_EVENT_QUIT:
                        for (Window& window : state.open_windows)
                            if (window.backend && window.status == WindowStatus::OPEN)
                                close_window(window);
                        break;
                    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                        if (const auto window = find_window(state, event.window.windowID))
                            close_window(window->get());
                        break;
                    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                        if (const auto window = find_window(state, event.window.windowID))
                        {
                            window->get().width = static_cast<uint32>(event.window.data1);
                            window->get().height = static_cast<uint32>(event.window.data2);
                            // The engine's render loop re-sizes the gpu drawable per window;
                            // the mirror tracks the main window for custom-pipeline hosts.
                            if (&window->get() == &state.open_windows.front())
                                set_render_viewport(event.window.data1, event.window.data2);
                            events.signal<WindowResized>().emit(
                                {.width = event.window.data1, .height = event.window.data2});
                        }
                        break;
                    default:
                        break;
                }
            }
            if (count < static_cast<int>(events_buffer.size()))
                break;
        }
    }

    void internal::make_current(const Window& window)
    {
        if (window.backend)
            SDL_GL_MakeCurrent(window.backend->window, internal::g_gl_context);
    }
}
