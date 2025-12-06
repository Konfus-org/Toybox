#include "sdl_windowing_plugin.h"
#include "tbx/app/application.h"
#include "tbx/debugging/macros.h"
#include "tbx/messages/observable.h"
#include <algorithm>
#include <string>
#include <utility>

namespace tbx::plugins::sdlwindowing
{
    void SdlWindowingPlugin::on_attach(Application&)
    {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
        {
            TBX_TRACE_ERROR("Failed to initialize SDL video subsystem. See SDL logs for details.");
            return;
        }
    }

    void SdlWindowingPlugin::on_detach()
    {
        for (auto& record : _windows)
        {
            if (record.sdl_window)
            {
                SDL_DestroyWindow(record.sdl_window);
            }
        }
        _windows.clear();
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    void SdlWindowingPlugin::on_update(const DeltaTime&)
    {
        SDL_Event event;
        if (SDL_PollEvent(&event))
        {
            // Window closed
            if (event.type == SDL_EventType::SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
                auto* window = SDL_GetWindowFromID(event.window.windowID);
                SdlWindowRecord record = find_record(window);
                record.tbx_window->is_open = false;
                remove_record(record);
            }
            // Window resized
            else if (event.type == SDL_EventType::SDL_EVENT_WINDOW_RESIZED)
            {
                auto* window = SDL_GetWindowFromID(event.window.windowID);
                SdlWindowRecord record = find_record(window);
                auto new_size = Size(
                    static_cast<uint>(event.window.data1),
                    static_cast<uint>(event.window.data2));
                record.tbx_window->size = new_size;
            }
            // Window minimized
            else if (event.type == SDL_EventType::SDL_EVENT_WINDOW_MINIMIZED)
            {
                auto* window = SDL_GetWindowFromID(event.window.windowID);
                SdlWindowRecord record = find_record(window);
                if (record.tbx_window)
                    record.tbx_window->mode = WindowMode::Minimized;
            }
            // Window restored
            else if (event.type == SDL_EventType::SDL_EVENT_WINDOW_RESTORED)
            {
                auto* window = SDL_GetWindowFromID(event.window.windowID);
                SdlWindowRecord record = find_record(window);
                if (record.tbx_window)
                    record.tbx_window->mode = WindowMode::Fullscreen;
            }
            // Window maximized
            else if (event.type == SDL_EventType::SDL_EVENT_WINDOW_MAXIMIZED)
            {
                auto* window = SDL_GetWindowFromID(event.window.windowID);
                SdlWindowRecord record = find_record(window);
                if (record.tbx_window)
                    record.tbx_window->mode = WindowMode::Fullscreen;
            }
            // Application quit
            else if (event.type == SDL_EventType::SDL_EVENT_QUIT)
            {
                for (auto& record : _windows)
                {
                    SDL_DestroyWindow(record.sdl_window);
                }
                _windows.clear();
            }
        }
    }

    void SdlWindowingPlugin::on_recieve_message(Message& msg)
    {
        // Window closed
        if (on_property_changed(
                msg,
                &Window::is_open,
                [this](PropertyChangedEvent<Window, bool>& event)
                {
                    on_window_is_open_changed(event);
                }))
        {
            return;
        }

        // Window resized
        if (on_property_changed(
                msg,
                &Window::size,
                [this](PropertyChangedEvent<Window, Size>& event)
                {
                    on_window_size_changed(event);
                }))
        {
            return;
        }

        // Window mode changed
        if (on_property_changed(
                msg,
                &Window::mode,
                [this](PropertyChangedEvent<Window, WindowMode>& event)
                {
                    on_window_mode_changed(event);
                }))
        {
            return;
        }

        // Window name changed
        if (on_property_changed(
                msg,
                &Window::title,
                [this](PropertyChangedEvent<Window, std::string>& event)
                {
                    on_window_title_changed(event);
                }))
        {
            return;
        }
    }

    void SdlWindowingPlugin::on_window_is_open_changed(PropertyChangedEvent<Window, bool>& event)
    {
        // Window is closing
        if (event.current == false)
        {
            SdlWindowRecord record = find_record(event.owner);
            SDL_DestroyWindow(record.sdl_window);
            remove_record(record);
        }
        // Window is opening
        else
        {
            Window* window = event.owner;

            std::string& title = window->title;
            Size& size = window->size;
            const WindowMode& mode = window->mode;

            uint flags = 0;
            if (window->mode == WindowMode::Borderless)
                flags |= SDL_WINDOW_BORDERLESS;
            else if (window->mode == WindowMode::Fullscreen)
                flags |= SDL_WINDOW_FULLSCREEN;
            else if (window->mode == WindowMode::Windowed)
                flags |= SDL_WINDOW_RESIZABLE;
            else if (window->mode == WindowMode::Minimized)
                flags |= SDL_WINDOW_MINIMIZED;
            SDL_Window* native = SDL_CreateWindow(title.c_str(), size.width, size.height, flags);
            if (!native)
            {
                TBX_TRACE_ERROR("Failed to create SDL window. See logs for details.");
                return;
            }

            add_record(native, window);
        }
    }

    void SdlWindowingPlugin::on_window_size_changed(PropertyChangedEvent<Window, Size>& event)
    {
        SdlWindowRecord record = find_record(event.owner);
        if (record.sdl_window)
        {
            SDL_SetWindowSize(
                record.sdl_window,
                static_cast<int>(event.current.width),
                static_cast<int>(event.current.height));
        }
    }

    void SdlWindowingPlugin::on_window_mode_changed(PropertyChangedEvent<Window, WindowMode>& event)
    {
        SdlWindowRecord record = find_record(event.owner);
        if (event.current == WindowMode::Borderless)
        {
            SDL_SetWindowBordered(record.sdl_window, false);
        }
        else if (event.current == WindowMode::Fullscreen)
        {
            SDL_SetWindowFullscreen(record.sdl_window, true);
        }
        else if (event.current == WindowMode::Minimized)
        {
            SDL_MinimizeWindow(record.sdl_window);
        }
        else if (event.current == WindowMode::Windowed)
        {
            SDL_SetWindowBordered(record.sdl_window, true);
            SDL_SetWindowFullscreen(record.sdl_window, false);
            SDL_RestoreWindow(record.sdl_window);
        }
        else
        {
            TBX_TRACE_WARNING("Unhandled window mode change!");
        }
    }

    void SdlWindowingPlugin::on_window_title_changed(
        PropertyChangedEvent<Window, std::string>& event)
    {
        SdlWindowRecord record = find_record(event.owner);
        if (record.sdl_window)
        {
            SDL_SetWindowTitle(record.sdl_window, event.current.c_str());
        }
    }

    SdlWindowRecord SdlWindowingPlugin::find_record(
        std::function<bool(const SdlWindowRecord&)> condition)
    {
        auto it = std::find_if(
            _windows.begin(),
            _windows.end(),
            [&condition](const SdlWindowRecord& record)
            {
                return condition(record);
            });
        if (it != _windows.end())
        {
            return *it;
        }
        return SdlWindowRecord();
    }

    SdlWindowRecord SdlWindowingPlugin::find_record(const SDL_Window* sdl_window)
    {
        return find_record(
            [&sdl_window](const SdlWindowRecord& record)
            {
                return record.sdl_window == sdl_window;
            });
    }

    SdlWindowRecord SdlWindowingPlugin::find_record(const Window* tbx_window)
    {
        return find_record(
            [&tbx_window](const SdlWindowRecord& record)
            {
                return record.tbx_window == tbx_window;
            });
    }

    SdlWindowRecord& SdlWindowingPlugin::add_record(SDL_Window* sdl_window, Window* tbx_window)
    {
        _windows.emplace_back();
        _windows.back().sdl_window = sdl_window;
        _windows.back().tbx_window = tbx_window;
        return _windows.back();
    }

    void SdlWindowingPlugin::remove_record(const SdlWindowRecord& record)
    {
        _windows.erase(
            std::ranges::remove_if(
                _windows,
                [&record](const SdlWindowRecord& rec)
                {
                    return rec.sdl_window == record.sdl_window;
                })
                .begin(),
            _windows.end());
    }
}
