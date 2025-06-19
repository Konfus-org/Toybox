#include "SDLWindow.h"
#include <Tbx/Events/WindowEvents.h>
#include <Tbx/Events/EventCoordinator.h>
#include <Tbx/Debug/Debugging.h>

namespace SDLWindowing
{
    SDLWindow::~SDLWindow()
    {
        Close();
    }

    Tbx::NativeHandle SDLWindow::GetNativeHandle() const
    {
        return 0;
    }

    Tbx::NativeWindow SDLWindow::GetNativeWindow() const
    {
        return nullptr;
    }

    void SDLWindow::Open(const Tbx::WindowMode& mode)
    {
        _currentMode = mode;

        Uint32 flags = SDL_WINDOW_RESIZABLE;
        switch (_currentMode)
        {
            case Tbx::WindowMode::Windowed: break;
            case Tbx::WindowMode::Fullscreen: flags |= SDL_WINDOW_FULLSCREEN; break;
            case Tbx::WindowMode::Borderless: flags |= SDL_WINDOW_BORDERLESS; break;
            case Tbx::WindowMode::FullscreenBorderless: 
            {
                flags |= SDL_WINDOW_FULLSCREEN;
                flags |= SDL_WINDOW_BORDERLESS; 
                break;
            }
        }

        _window = SDL_CreateWindow(_title.c_str(), _size.Width, _size.Height, flags);
        TBX_ASSERT(_window, "SDL_CreateWindow failed: %s", SDL_GetError());

        auto e = Tbx::WindowOpenedEvent(GetId());
        Tbx::EventCoordinator::Send(e);
    }

    void SDLWindow::Close()
    {
        SDL_DestroyWindow(_window);
        _window = nullptr;

        auto e = Tbx::WindowClosedEvent(GetId());
        Tbx::EventCoordinator::Send(e);
    }

    void SDLWindow::Update()
    {
        SDL_Event e;
        SDL_PollEvent(&e);
        if (e.type == SDL_EVENT_QUIT) Close();
    }

    void SDLWindow::Focus()
    {
        SDL_RaiseWindow(_window);

        auto e = Tbx::WindowFocusChangedEvent(GetId(), true);
        Tbx::EventCoordinator::Send(e);
    }

    const std::string& SDLWindow::GetTitle() const
    {
        return _title;
    }

    void SDLWindow::SetTitle(const std::string& title)
    {
        _title = title;
        SDL_SetWindowTitle(_window, title.c_str());
    }

    const Tbx::Size& SDLWindow::GetSize() const
    {
        return _size;
    }

    void SDLWindow::SetSize(const Tbx::Size& size)
    {
        _size = size;
        SDL_SetWindowSize(_window, _size.Width, _size.Height);
    }

    void SDLWindow::SetMode(const Tbx::WindowMode& mode)
    {
        _currentMode = mode;
        switch (_currentMode)
        {
            case Tbx::WindowMode::Windowed:
            {
                SDL_SetWindowFullscreen(_window, false);
                SDL_SetWindowBordered(_window, true);
                break;
            }
            case Tbx::WindowMode::Fullscreen:
            {
                SDL_SetWindowFullscreen(_window, true);
                break;
            }
            case Tbx::WindowMode::Borderless:
            {
                SDL_SetWindowFullscreen(_window, false);
                SDL_SetWindowBordered(_window, false);
                break;
            }
            case Tbx::WindowMode::FullscreenBorderless:
            {
                SDL_SetWindowFullscreen(_window, true);
                SDL_SetWindowBordered(_window, false);
                break;
            }
        }
    }
}