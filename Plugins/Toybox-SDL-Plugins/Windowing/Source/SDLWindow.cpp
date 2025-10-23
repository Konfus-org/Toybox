#include "SDLWindow.h"
#include "Tbx/Debug/Asserts.h"
#include "Tbx/Events/WindowEvents.h"
#include <SDL3/SDL_events.h>

namespace Tbx::Plugins::SDLWindowing
{
    SDLWindow::SDLWindow(bool useOpenGl, Ref<EventBus> eventBus)
        : _eventCarrier(eventBus), _useOpenGl(useOpenGl)
    {
    }

    SDLWindow::~SDLWindow()
    {
        Close();
    }

    NativeHandle SDLWindow::GetNativeHandle() const
    {
        return SDL_GetDisplayForWindow(_window);
    }

    NativeWindow SDLWindow::GetNativeWindow() const
    {
        return _window;
    }

    void SDLWindow::Open()
    {
        _isClosed = false;
        Uint32 flags = SDL_WINDOW_RESIZABLE;

        if (_useOpenGl)
        {
            flags |= SDL_WINDOW_OPENGL;
        }

        switch (_currentMode)
        {
            using enum WindowMode;
            case Windowed:   break;
            case Fullscreen: flags |= SDL_WINDOW_FULLSCREEN; break;
            case Borderless: flags |= SDL_WINDOW_BORDERLESS; break;
            case FullscreenBorderless:
            {
                flags |= SDL_WINDOW_FULLSCREEN;
                flags |= SDL_WINDOW_BORDERLESS;
                break;
            }
            default:
            {
                TBX_ASSERT(false, "Invalid window mode");
            }
            _eventCarrier.Post(WindowModeChangedEvent(this));
        }

        _window = SDL_CreateWindow(_title.c_str(), _size.Width, _size.Height, flags);
        TBX_ASSERT(_window, "SDLWindow: SDL_CreateWindow failed: %s", SDL_GetError());
        _eventCarrier.Post(WindowOpenedEvent(this));
    }

    void SDLWindow::Close()
    {
        // We've already been closed...
        if (_window == nullptr) return;

        // Cleanup...
        SDL_DestroyWindow(_window);
        _window = nullptr;
        _isClosed = true;

        if (_useOpenGl)
        {
            // Reset the context
            SDL_GL_DestroyContext(_glContext);
        }

        _eventCarrier.Post(WindowClosedEvent(this));
    }

    void SDLWindow::Update()
    {
        SDL_Event e = {};
        SDL_PollEvent(&e);
        switch (e.type)
        {
            case SDL_EVENT_QUIT:
            {
                Close();
                break;
            }
        }

        int w, h;
        SDL_GetWindowSize(_window, &w, &h);
        SetSize(Size(w, h));

        auto flags = SDL_GetWindowFlags(_window);
        if (flags & SDL_WINDOW_FULLSCREEN)
        {
            SetMode(WindowMode::Fullscreen);
        }
        else if (flags & SDL_WINDOW_FULLSCREEN & SDL_WINDOW_BORDERLESS)
        {
            SetMode(WindowMode::FullscreenBorderless);
        }
        else if (flags & SDL_WINDOW_BORDERLESS)
        {
            SetMode(WindowMode::Borderless);
        }
        else if (flags & SDL_WINDOW_MINIMIZED)
        {
            SetMode(WindowMode::Minimized);
        }
        else
        {
            SetMode(WindowMode::Windowed);
        }

        if (flags & SDL_WINDOW_INPUT_FOCUS)
        {
            Focus();
        }
        else
        {
            _isFocused = false;
        }
    }

    void SDLWindow::Focus()
    {
        if (_isFocused)
        {
            return;
        }

        _isFocused = true;
        SDL_RaiseWindow(_window);
        if (_useOpenGl)
        {
            SDL_GL_MakeCurrent(_window, _glContext);
        }
        _eventCarrier.Post(WindowFocusedEvent(this));
    }

    bool SDLWindow::IsClosed()
    {
        return _isClosed;
    }

    bool SDLWindow::IsFocused()
    {
        return _isFocused;
    }

    const std::string& SDLWindow::GetTitle() const
    {
        return _title;
    }

    void SDLWindow::SetTitle(const std::string& title)
    {
        _title = title;

        if (!_window)
        {
            return;
        }

        SDL_SetWindowTitle(_window, title.c_str());
    }

    const Size& SDLWindow::GetSize() const
    {
        return _size;
    }

    void SDLWindow::SetSize(const Size& size)
    {
        _size = size;
        if (_size.GetAspectRatio() == size.GetAspectRatio())
        {
            return;
        }

        SDL_SetWindowSize(_window, _size.Width, _size.Height);
        _eventCarrier.Post(WindowResizedEvent(this));
    }

    void SDLWindow::SetMode(const WindowMode& mode)
    {
        if (mode == _currentMode)
        {
            return;
        }

        _currentMode = mode;
        if (_window == nullptr)
        {
            return;
        }

        switch (_currentMode)
        {
            using enum WindowMode;
            case Windowed:
            {
                SDL_SetWindowFullscreen(_window, false);
                SDL_SetWindowBordered(_window, true);
                break;
            }
            case Fullscreen:
            {
                SDL_SetWindowFullscreen(_window, true);
                break;
            }
            case Borderless:
            {
                SDL_SetWindowFullscreen(_window, false);
                SDL_SetWindowBordered(_window, false);
                break;
            }
            case FullscreenBorderless:
            {
                SDL_SetWindowFullscreen(_window, true);
                SDL_SetWindowBordered(_window, false);
                break;
            }
            case Minimized:
            {
                SDL_MinimizeWindow(_window);
                break;
            }
        }

        _eventCarrier.Post(WindowModeChangedEvent(this));
    }

    WindowMode SDLWindow::GetMode() const
    {
        return _currentMode;
    }
}