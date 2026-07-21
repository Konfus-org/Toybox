#include "tbx/platform/window.h"
#include "tbx/debug/log.h"
#include "tbx/platform/input.h"
#include <SDL3/SDL.h>

namespace tbx
{
    /// @brief
    /// Purpose: SDL-side window state; lives behind the Window boundary so SDL types never
    /// escape this backend folder.
    struct Window::State
    {
        SDL_Window* window = nullptr;
        SDL_GLContext gl_context = nullptr;
        int width = 0;
        int height = 0;
        bool is_headless = false;
    };

    //// TRANSLATION ////

    static Key translate_key(SDL_Scancode scancode)
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

    static MouseButton translate_mouse_button(Uint8 button)
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

    //// WINDOW ////

    Window::Window(const WindowDescription& description)
    {
        _state = std::make_unique<State>();
        _state->width = description.width;
        _state->height = description.height;
        _state->is_headless = description.is_headless;
        if (description.is_headless)
            return;

        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            log_error("SDL_Init failed: {}", SDL_GetError());
            std::abort();
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

        _state->window = SDL_CreateWindow(
            description.title.c_str(),
            description.width,
            description.height,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        if (!_state->window)
        {
            log_error("SDL_CreateWindow failed: {}", SDL_GetError());
            std::abort();
        }

        _state->gl_context = SDL_GL_CreateContext(_state->window);
        if (!_state->gl_context)
        {
            log_error("SDL_GL_CreateContext failed: {}", SDL_GetError());
            std::abort();
        }
        SDL_GL_SetSwapInterval(1);
        SDL_GetWindowSizeInPixels(_state->window, &_state->width, &_state->height);
    }

    Window::~Window()
    {
        if (_state->gl_context)
            SDL_GL_DestroyContext(_state->gl_context);
        if (_state->window)
        {
            SDL_DestroyWindow(_state->window);
            SDL_Quit();
        }
    }

    bool Window::is_headless() const
    {
        return _state->is_headless;
    }

    int Window::get_height() const
    {
        return _state->height;
    }

    bool Window::pump(Events& events)
    {
        if (_state->is_headless)
            return true;

        auto event = SDL_Event {};
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_EVENT_QUIT:
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    return false;
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP:
                {
                    const Key key = translate_key(event.key.scancode);
                    if (key == Key::UNKNOWN)
                        break;
                    if (!event.key.repeat)
                        input::feed_key(key, event.key.down);
                    events.key.emit(
                        {.key = key, .is_down = event.key.down, .is_repeat = event.key.repeat != 0});
                    break;
                }
                case SDL_EVENT_MOUSE_MOTION:
                    input::feed_mouse_move(
                        Vec2(event.motion.x, event.motion.y),
                        Vec2(event.motion.xrel, event.motion.yrel));
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP:
                {
                    const MouseButton button = translate_mouse_button(event.button.button);
                    if (button != MouseButton::COUNT)
                        input::feed_mouse_button(button, event.button.down);
                    break;
                }
                case SDL_EVENT_MOUSE_WHEEL:
                    input::feed_scroll(event.wheel.y);
                    break;
                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                    _state->width = event.window.data1;
                    _state->height = event.window.data2;
                    events.window_resized.emit(
                        {.width = _state->width, .height = _state->height});
                    break;
                default:
                    break;
            }
        }
        return true;
    }

    void Window::set_icon(
        const int width,
        const int height,
        const std::span<const std::byte> rgba_pixels)
    {
        if (!_state->window || width <= 0 || height <= 0
            || rgba_pixels.size() < static_cast<size>(width) * height * 4)
            return;
        SDL_Surface* surface = SDL_CreateSurfaceFrom(
            width,
            height,
            SDL_PIXELFORMAT_RGBA32,
            const_cast<std::byte*>(rgba_pixels.data()),
            width * 4);
        if (!surface)
        {
            log_warn("window icon surface failed: {}", SDL_GetError());
            return;
        }
        SDL_SetWindowIcon(_state->window, surface);
        SDL_DestroySurface(surface);
    }

    void Window::swap()
    {
        if (_state->window)
            SDL_GL_SwapWindow(_state->window);
    }

    int Window::get_width() const
    {
        return _state->width;
    }
}
