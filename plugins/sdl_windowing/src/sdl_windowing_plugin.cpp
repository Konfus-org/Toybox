#include "sdl_windowing_plugin.h"
#include "tbx/app/application.h"
#include "tbx/debugging/macros.h"
#include "tbx/messages/observable.h"
#include <algorithm>

namespace tbx::plugins
{
    static WindowMode get_window_mode_from_flags(
        SDL_Window* sdl_window,
        WindowMode fallback_mode = WindowMode::WINDOWED)
    {
        if (!sdl_window)
        {
            return fallback_mode;
        }

        auto flags = SDL_GetWindowFlags(sdl_window);
        if ((flags & SDL_WINDOW_FULLSCREEN) != 0)
        {
            return WindowMode::FULLSCREEN;
        }
        if ((flags & SDL_WINDOW_MINIMIZED) != 0)
        {
            return WindowMode::MINIMIZED;
        }
        if ((flags & SDL_WINDOW_BORDERLESS) != 0)
        {
            return WindowMode::BORDERLESS;
        }
        return WindowMode::WINDOWED;
    }

    static SDL_Window* create_sdl_window(Window* tbx_window, bool use_opengl)
    {
        uint flags = use_opengl ? SDL_WINDOW_OPENGL : 0;
        if (tbx_window->mode == WindowMode::BORDERLESS)
            flags |= SDL_WINDOW_BORDERLESS;
        else if (tbx_window->mode == WindowMode::FULLSCREEN)
            flags |= SDL_WINDOW_FULLSCREEN;
        else if (tbx_window->mode == WindowMode::WINDOWED)
            flags |= SDL_WINDOW_RESIZABLE;
        else if (tbx_window->mode == WindowMode::MINIMIZED)
            flags |= SDL_WINDOW_MINIMIZED;

        std::string& title = tbx_window->title;
        Size& size = tbx_window->size;
        SDL_Window* native = SDL_CreateWindow(title.c_str(), size.width, size.height, flags);
        if (!native)
        {
            TBX_ASSERT(false, "Failed to create SDL window!");
            return nullptr;
        }

        return native;
    }

    void SdlWindowingPlugin::on_attach(IPluginHost& host)
    {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
        {
            TBX_TRACE_ERROR("Failed to initialize SDL video subsystem. See SDL logs for details.");
            return;
        }
        else
            TBX_TRACE_INFO("Initialized SDL video subsystem.");

        _use_opengl = host.get_settings().graphics_api == GraphicsApi::OPEN_GL;
    }

    void SdlWindowingPlugin::on_detach()
    {
        for (auto& record : _windows)
        {
            if (record.tbx_window && record.sdl_window)
            {
                send_message<WindowNativeHandleChangedEvent>(
                    record.tbx_window->id,
                    record.sdl_window,
                    nullptr,
                    record.tbx_window->size.value);
            }
            SDL_DestroyWindow(record.sdl_window);
        }
        _windows.clear();
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    void SdlWindowingPlugin::on_update(const DeltaTime&)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_EventType::SDL_EVENT_WINDOW_FOCUS_GAINED:
                {
                    break;
                }
                case SDL_EventType::SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                {
                    auto* window = SDL_GetWindowFromID(event.window.windowID);
                    SdlWindowRecord* record = try_get_record(window);
                    if (record && record->tbx_window)
                        record->tbx_window->is_open = false;
                    break;
                }
                case SDL_EventType::SDL_EVENT_WINDOW_RESIZED:
                {
                    auto* window = SDL_GetWindowFromID(event.window.windowID);
                    SdlWindowRecord* record = try_get_record(window);
                    if (record && record->tbx_window)
                    {
                        auto new_size = Size(
                            static_cast<uint>(event.window.data1),
                            static_cast<uint>(event.window.data2));
                        record->tbx_window->size = new_size;
                    }
                    break;
                }
                case SDL_EventType::SDL_EVENT_WINDOW_MINIMIZED:
                {
                    auto* window = SDL_GetWindowFromID(event.window.windowID);
                    SdlWindowRecord* record = try_get_record(window);
                    if (record && record->tbx_window)
                    {
                        record->mode_to_restore = get_window_mode_from_flags(record->sdl_window);
                        record->tbx_window->mode = WindowMode::MINIMIZED;
                    }
                    break;
                }
                case SDL_EventType::SDL_EVENT_WINDOW_RESTORED:
                {
                    auto* window = SDL_GetWindowFromID(event.window.windowID);
                    SdlWindowRecord* record = try_get_record(window);
                    if (record && record->tbx_window)
                        record->tbx_window->mode = record->mode_to_restore;
                    break;
                }
                case SDL_EventType::SDL_EVENT_QUIT:
                {
                    for (auto& record : _windows)
                    {
                        if (record.tbx_window && record.sdl_window)
                        {
                            send_message<WindowNativeHandleChangedEvent>(
                                record.tbx_window->id,
                                record.sdl_window,
                                nullptr,
                                record.tbx_window->size.value);
                        }
                        SDL_DestroyWindow(record.sdl_window);
                    }
                    _windows.clear();
                    break;
                }
                default:
                    break;
            }
        }
    }

    void SdlWindowingPlugin::on_recieve_message(Message& msg)
    {
        if (auto* snapshot_request = handle_message<WindowNativeHandleSnapshotRequest>(msg))
        {
            for (const auto& record : _windows)
            {
                if (!record.tbx_window || !record.sdl_window)
                    continue;

                send_message<WindowNativeHandleChangedEvent>(
                    record.tbx_window->id,
                    nullptr,
                    record.sdl_window,
                    record.tbx_window->size.value);
            }

            snapshot_request->state = MessageState::HANDLED;
            snapshot_request->result.flag_success();
            return;
        }

        // Graphics api changed
        if (auto* graphics_event = handle_property_changed<&AppSettings::graphics_api>(msg))
        {
            _use_opengl = graphics_event->current == GraphicsApi::OPEN_GL;

            // Recreate all windows with new graphics api
            for (auto& record : _windows)
            {
                // Destory old SDL window
                if (record.tbx_window && record.sdl_window)
                {
                    send_message<WindowNativeHandleChangedEvent>(
                        record.tbx_window->id,
                        record.sdl_window,
                        nullptr,
                        record.tbx_window->size.value);
                }
                SDL_DestroyWindow(record.sdl_window);
                record.sdl_window = nullptr;

                // Create new SDL window
                Window* window = record.tbx_window;
                SDL_Window* native = create_sdl_window(window, _use_opengl);
                record.sdl_window = native;

                if (window && native)
                    send_message<WindowNativeHandleChangedEvent>(
                        window->id,
                        nullptr,
                        native,
                        window->size.value);
            }
            return;
        }

        // Window closed
        if (auto* open_event = handle_property_changed<&Window::is_open>(msg))
        {
            on_window_is_open_changed(*open_event);
            return;
        }

        // Window resized
        if (auto* size_event = handle_property_changed<&Window::size>(msg))
        {
            on_window_size_changed(*size_event);
            return;
        }

        // Window mode changed
        if (auto* mode_event = handle_property_changed<&Window::mode>(msg))
        {
            on_window_mode_changed(*mode_event);
            return;
        }

        // Window name changed
        if (auto* title_event = handle_property_changed<&Window::title>(msg))
        {
            on_window_title_changed(*title_event);
            return;
        }
    }

    void SdlWindowingPlugin::on_window_is_open_changed(PropertyChangedEvent<Window, bool>& event)
    {
        // Window is closing
        if (event.current == false)
        {
            SdlWindowRecord* record = try_get_record(event.owner);
            if (!record)
            {
                return;
            }
            if (record->tbx_window && record->sdl_window)
            {
                send_message<WindowNativeHandleChangedEvent>(
                    record->tbx_window->id,
                    record->sdl_window,
                    nullptr,
                    record->tbx_window->size.value);
            }
            SDL_DestroyWindow(record->sdl_window);
            remove_record(*record);
        }
        // Window is opening
        else
        {
            Window* window = event.owner;
            SDL_Window* native = create_sdl_window(window, _use_opengl);
            SdlWindowRecord& record = add_record(native, window);
            (void)record;

            if (window && native)
                send_message<WindowNativeHandleChangedEvent>(
                    window->id,
                    nullptr,
                    native,
                    window->size.value);
        }
    }

    void SdlWindowingPlugin::on_window_size_changed(PropertyChangedEvent<Window, Size>& event)
    {
        SdlWindowRecord* record = try_get_record(event.owner);
        if (record && record->sdl_window)
        {
            SDL_SetWindowSize(
                record->sdl_window,
                static_cast<int>(event.current.width),
                static_cast<int>(event.current.height));
        }
    }

    void SdlWindowingPlugin::on_window_mode_changed(PropertyChangedEvent<Window, WindowMode>& event)
    {
        SdlWindowRecord* record = try_get_record(event.owner);
        if (!record || !record->sdl_window)
        {
            return;
        }

        record->mode_to_restore = event.previous;
        if (event.current == WindowMode::BORDERLESS)
            SDL_SetWindowBordered(record->sdl_window, false);
        else if (event.current == WindowMode::FULLSCREEN)
            SDL_SetWindowFullscreen(record->sdl_window, true);
        else if (event.current == WindowMode::MINIMIZED)
            SDL_MinimizeWindow(record->sdl_window);
        else if (event.current == WindowMode::WINDOWED)
        {
            SDL_SetWindowBordered(record->sdl_window, true);
            SDL_SetWindowFullscreen(record->sdl_window, false);
            SDL_RestoreWindow(record->sdl_window);
        }
        else
            TBX_ASSERT(false, "Unhandled window mode change!");
    }

    void SdlWindowingPlugin::on_window_title_changed(
        PropertyChangedEvent<Window, std::string>& event)
    {
        SdlWindowRecord* record = try_get_record(event.owner);
        if (record && record->sdl_window)
        {
            SDL_SetWindowTitle(record->sdl_window, event.current.c_str());
        }
    }

    SdlWindowRecord* SdlWindowingPlugin::try_get_record(
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
            return &(*it);
        }
        return nullptr;
    }

    SdlWindowRecord* SdlWindowingPlugin::try_get_record(const SDL_Window* sdl_window)
    {
        return try_get_record(
            [&sdl_window](const SdlWindowRecord& record)
            {
                return record.sdl_window == sdl_window;
            });
    }

    SdlWindowRecord* SdlWindowingPlugin::try_get_record(const Window* tbx_window)
    {
        return try_get_record(
            [&tbx_window](const SdlWindowRecord& record)
            {
                return record.tbx_window == tbx_window;
            });
    }

    SdlWindowRecord* SdlWindowingPlugin::try_get_record(const Uuid& window_id)
    {
        return try_get_record(
            [&window_id](const SdlWindowRecord& record)
            {
                return record.tbx_window && record.tbx_window->id == window_id;
            });
    }

    SdlWindowRecord& SdlWindowingPlugin::add_record(SDL_Window* sdl_window, Window* tbx_window)
    {
        _windows.emplace_back();
        _windows.back().sdl_window = sdl_window;
        _windows.back().tbx_window = tbx_window;
        if (tbx_window)
        {
            _windows.back().mode_to_restore = tbx_window->mode;
        }
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
