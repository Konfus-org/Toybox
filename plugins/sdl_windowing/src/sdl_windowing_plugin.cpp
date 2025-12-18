#include "sdl_windowing_plugin.h"
#include "tbx/app/application.h"
#include "tbx/debugging/macros.h"
#include "tbx/messages/observable.h"
#include <algorithm>
#include <utility>

namespace tbx::plugins
{
    static SDL_Window* CreateSdlWindow(Window* tbx_window, bool use_opengl)
    {
        String& title = tbx_window->title;
        Size& size = tbx_window->size;
        const WindowMode& mode = tbx_window->mode;

        uint flags = 0;
        if (tbx_window->mode == WindowMode::Borderless)
            flags |= SDL_WINDOW_BORDERLESS;
        else if (tbx_window->mode == WindowMode::Fullscreen)
            flags |= SDL_WINDOW_FULLSCREEN;
        else if (tbx_window->mode == WindowMode::Windowed)
            flags |= SDL_WINDOW_RESIZABLE;
        else if (tbx_window->mode == WindowMode::Minimized)
            flags |= SDL_WINDOW_MINIMIZED;
        if (use_opengl)
            flags |= SDL_WINDOW_OPENGL;
        SDL_Window* native = SDL_CreateWindow(title.get_cstr(), size.width, size.height, flags);

        if (!native)
        {
            TBX_ASSERT(false, "Failed to create SDL window!");
            return nullptr;
        }

        return native;
    }

    void SdlWindowingPlugin::on_attach(Application& host)
    {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
        {
            TBX_TRACE_ERROR("Failed to initialize SDL video subsystem. See SDL logs for details.");
            return;
        }

        _use_opengl = host.get_settings().graphics_api == GraphicsApi::OpenGL;
    }

    void SdlWindowingPlugin::on_detach()
    {
        for (auto& record : _windows)
        {
            SDL_DestroyWindow(record.sdl_window);
        }
        _windows.clear();
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    void SdlWindowingPlugin::on_update(const DeltaTime&)
    {
        SDL_Event event;
        if (!SDL_PollEvent(&event))
            return;

        switch (event.type)
        {
            case SDL_EventType::SDL_EVENT_WINDOW_FOCUS_GAINED:
            {
                if (_use_opengl)
                {
                    // if using opengl, make context current
                    SDL_GL_MakeCurrent(
                        SDL_GetWindowFromID(event.window.windowID),
                        SDL_GL_GetCurrentContext());
                }
                break;
            }
            case SDL_EventType::SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            {
                auto* window = SDL_GetWindowFromID(event.window.windowID);
                SdlWindowRecord record = find_record(window);
                record.tbx_window->is_open = false;
                remove_record(record);
                break;
            }
            case SDL_EventType::SDL_EVENT_WINDOW_RESIZED:
            {
                auto* window = SDL_GetWindowFromID(event.window.windowID);
                SdlWindowRecord record = find_record(window);
                auto new_size = Size(
                    static_cast<uint>(event.window.data1),
                    static_cast<uint>(event.window.data2));
                record.tbx_window->size = new_size;
                break;
            }
            case SDL_EventType::SDL_EVENT_WINDOW_MINIMIZED:
            {
                auto* window = SDL_GetWindowFromID(event.window.windowID);
                SdlWindowRecord record = find_record(window);
                if (record.tbx_window)
                    record.tbx_window->mode = WindowMode::Minimized;
                break;
            }
            case SDL_EventType::SDL_EVENT_WINDOW_RESTORED:
            {
                auto* window = SDL_GetWindowFromID(event.window.windowID);
                SdlWindowRecord record = find_record(window);
                if (record.tbx_window)
                    record.tbx_window->mode = WindowMode::Fullscreen;
                break;
            }
            case SDL_EventType::SDL_EVENT_WINDOW_MAXIMIZED:
            {
                auto* window = SDL_GetWindowFromID(event.window.windowID);
                SdlWindowRecord record = find_record(window);
                if (record.tbx_window)
                    record.tbx_window->mode = WindowMode::Fullscreen;
                break;
            }
            case SDL_EventType::SDL_EVENT_QUIT:
            {
                for (auto& record : _windows)
                {
                    SDL_DestroyWindow(record.sdl_window);
                }
                _windows.clear();
                break;
            }
            default:
                break;
        }
    }

    void SdlWindowingPlugin::on_recieve_message(Message& msg)
    {
        // Graphics api changed
        if (on_property_changed(
                msg,
                &AppSettings::graphics_api,
                [this](PropertyChangedEvent<AppSettings, GraphicsApi>& event)
                {
                    _use_opengl = event.current == GraphicsApi::OpenGL;

                    // Recreate all windows with new graphics api
                    for (auto& record : _windows)
                    {
                        // Destory old SDL window
                        SDL_DestroyWindow(record.sdl_window);
                        record.sdl_window = nullptr;

                        // Create new SDL window
                        Window* window = record.tbx_window;
                        SDL_Window* native = CreateSdlWindow(window, _use_opengl);
                        record.sdl_window = native;
                    }
                }))
        {
            return;
        }

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
                [this](PropertyChangedEvent<Window, String>& event)
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
            SDL_Window* native = CreateSdlWindow(window, _use_opengl);
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
            SDL_SetWindowBordered(record.sdl_window, false);
        else if (event.current == WindowMode::Fullscreen)
            SDL_SetWindowFullscreen(record.sdl_window, true);
        else if (event.current == WindowMode::Minimized)
            SDL_MinimizeWindow(record.sdl_window);
        else if (event.current == WindowMode::Windowed)
        {
            SDL_SetWindowBordered(record.sdl_window, true);
            SDL_SetWindowFullscreen(record.sdl_window, false);
            SDL_RestoreWindow(record.sdl_window);
        }
        else
            TBX_ASSERT(false, "Unhandled window mode change!");
    }

    void SdlWindowingPlugin::on_window_title_changed(PropertyChangedEvent<Window, String>& event)
    {
        SdlWindowRecord record = find_record(event.owner);
        if (record.sdl_window)
        {
            SDL_SetWindowTitle(record.sdl_window, event.current.get_cstr());
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
