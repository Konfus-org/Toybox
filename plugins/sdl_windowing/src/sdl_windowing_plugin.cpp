#include "sdl_windowing_plugin.h"
#include "tbx/app/application.h"
#include "tbx/debugging/macros.h"
#include "tbx/messages/observable.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string_view>

namespace sdl_windowing
{
    using namespace tbx;
    static bool is_wayland_video_driver()
    {
        const char* video_driver = SDL_GetCurrentVideoDriver();
        return video_driver != nullptr && std::string_view(video_driver) == "wayland";
    }

    static SDL_Surface* try_load_icon_surface(const std::filesystem::path& icon_path)
    {
        if (icon_path.empty())
            return nullptr;

        // Wayland doesn't support icons on windows so no need to load it up.
        if (is_wayland_video_driver())
            return nullptr;

        SDL_ClearError();
        SDL_Surface* icon_surface = SDL_LoadSurface(icon_path.string().c_str());
        if (icon_surface)
        {
            TBX_TRACE_INFO("Loaded app icon '{}'.", icon_path.string());
            return icon_surface;
        }

        TBX_TRACE_WARNING(
            "Failed to load app icon '{}'. Error: {}",
            icon_path.string(),
            SDL_GetError());
        SDL_ClearError();
        return nullptr;
    }

    static void try_apply_window_icon(SDL_Window* native, SDL_Surface* icon_surface)
    {
        if (!native || !icon_surface)
            return;

        // Wayland video drivers do not support setting window icons, so we skip this step on
        // Wayland systems.
        if (is_wayland_video_driver())
            return;

        if (!SDL_SetWindowIcon(native, icon_surface))
        {
            TBX_TRACE_WARNING("Failed to set SDL window icon. Error: {}", SDL_GetError());
            SDL_ClearError();
        }
    }

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

    static SDL_Window* create_sdl_window(
        Window* tbx_window,
        bool use_opengl,
        SDL_Surface* icon_surface)
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
            TBX_ASSERT(false, "Failed to create SDL window! Error {}", SDL_GetError());
            return nullptr;
        }

        TBX_TRACE_INFO(
            "Created window with title '{}', size ({}, {})",
            title,
            size.width,
            size.height);
        try_apply_window_icon(native, icon_surface);
        return native;
    }

    void SdlWindowingPlugin::on_attach(IPluginHost& host)
    {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
        {
            TBX_TRACE_ERROR("Failed to initialize SDL video subsystem. Error: {}", SDL_GetError());
            return;
        }
        else
            TBX_TRACE_INFO("Initialized SDL video subsystem.");

        TBX_TRACE_INFO("Video driver: {}", SDL_GetCurrentVideoDriver());
        _use_opengl = host.get_settings().graphics.graphics_api == GraphicsApi::OPEN_GL;

        const std::filesystem::path icon_path =
            host.get_asset_manager().resolve_asset_path(host.get_icon_handle());
        if (icon_path.empty())
        {
            TBX_TRACE_WARNING(
                "Failed to resolve app icon handle to a path. Window icon will not be set.");
            return;
        }

        _window_icon_surface = try_load_icon_surface(icon_path);
    }

    void SdlWindowingPlugin::on_detach()
    {
        _pending_close_window_ids.clear();
        for (auto& record : _windows)
        {
            if (record.tbx_window)
                record.tbx_window->native_handle = nullptr;
            SDL_DestroyWindow(record.sdl_window);
        }
        _windows.clear();
        if (_window_icon_surface)
        {
            SDL_DestroySurface(_window_icon_surface);
            _window_icon_surface = nullptr;
        }
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
                        if (record.tbx_window)
                            record.tbx_window->native_handle = nullptr;
                        SDL_DestroyWindow(record.sdl_window);
                    }
                    _windows.clear();
                    break;
                }
                default:
                    break;
            }
        }

        process_pending_window_closes();
    }

    void SdlWindowingPlugin::on_recieve_message(Message& msg)
    {
        // Graphics api changed
        if (auto* graphics_event = handle_property_changed<&GraphicsSettings::graphics_api>(msg))
        {
            _use_opengl = graphics_event->current == GraphicsApi::OPEN_GL;

            // Recreate all windows with new graphics api
            for (auto& record : _windows)
            {
                // Destory old SDL window
                if (record.tbx_window)
                    record.tbx_window->native_handle = nullptr;
                SDL_DestroyWindow(record.sdl_window);
                record.sdl_window = nullptr;

                // Create new SDL window
                Window* window = record.tbx_window;
                SDL_Window* native = create_sdl_window(window, _use_opengl, _window_icon_surface);
                record.sdl_window = native;
                if (window)
                    window->native_handle = native;
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
                return;

            const Uuid window_id = record->tbx_window ? record->tbx_window->id : Uuid::NONE;
            if (window_id.is_valid()
                && !std::ranges::contains(_pending_close_window_ids, window_id))
                _pending_close_window_ids.push_back(window_id);
        }
        // Window is opening
        else
        {
            if (!event.owner)
                return;

            auto pending_it = std::ranges::find(_pending_close_window_ids, event.owner->id);
            if (pending_it != _pending_close_window_ids.end())
                _pending_close_window_ids.erase(pending_it);

            if (try_get_record(event.owner))
                return;

            Window* window = event.owner;
            SDL_Window* native = create_sdl_window(window, _use_opengl, _window_icon_surface);
            SdlWindowRecord& record = add_record(native, window);
            (void)record;
            if (window)
                window->native_handle = native;
        }
    }

    void SdlWindowingPlugin::process_pending_window_closes()
    {
        if (_pending_close_window_ids.empty())
            return;

        auto closing_window_ids = std::move(_pending_close_window_ids);
        _pending_close_window_ids.clear();

        for (const Uuid& window_id : closing_window_ids)
        {
            SdlWindowRecord* record = try_get_record(window_id);
            if (!record)
                continue;

            if (record->tbx_window)
                record->tbx_window->native_handle = nullptr;
            SDL_DestroyWindow(record->sdl_window);
            remove_record(*record);
        }
    }

    void SdlWindowingPlugin::on_window_size_changed(PropertyChangedEvent<Window, Size>& event)
    {
        SdlWindowRecord* record = try_get_record(event.owner);
        if (record && record->sdl_window)
        {
            int current_width = 0;
            int current_height = 0;
            if (!SDL_GetWindowSize(record->sdl_window, &current_width, &current_height))
            {
                TBX_TRACE_WARNING(
                    "Failed to query SDL window size for '{}': {}",
                    record->tbx_window ? record->tbx_window->title.value : "Unnamed Window",
                    SDL_GetError());
                return;
            }

            int target_width = static_cast<int>(event.current.width);
            int target_height = static_cast<int>(event.current.height);
            if (current_width == target_width && current_height == target_height)
                return;

            SDL_SetWindowSize(record->sdl_window, target_width, target_height);
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
