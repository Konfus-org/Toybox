#include "sdl_window_manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/messages.h"
#include "tbx/types/handle.h"
#include "tbx/types/typedefs.h"
#include <algorithm>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>


namespace sdl_windowing
{
    namespace
    {
        bool are_sizes_equal(const tbx::Size& left, const tbx::Size& right)
        {
            return left.width == right.width && left.height == right.height;
        }

        const char* to_string(const tbx::WindowMode mode)
        {
            switch (mode)
            {
                case tbx::WindowMode::WINDOWED:
                    return "Windowed";
                case tbx::WindowMode::BORDERLESS:
                    return "Borderless";
                case tbx::WindowMode::FULLSCREEN:
                    return "Fullscreen";
                case tbx::WindowMode::MINIMIZED:
                    return "Minimized";
            }

            return "Unknown";
        }

        std::string to_string(const tbx::Size& size)
        {
            return std::to_string(size.width) + "x" + std::to_string(size.height);
        }

        std::string describe_window(const tbx::Window& window, std::string_view title)
        {
            auto description = tbx::to_string(window);
            description += " ('";
            description += title;
            description += "')";
            return description;
        }

        std::string sanitize_window_handle_name(std::string title)
        {
            if (title.empty())
                return "Toybox";

            return title;
        }

        bool is_wayland_video_driver()
        {
            const char* video_driver = SDL_GetCurrentVideoDriver();
            return video_driver != nullptr && std::string_view(video_driver) == "wayland";
        }

        void try_apply_window_icon(SDL_Window* native, SDL_Surface* icon_surface)
        {
            if (!native || !icon_surface)
                return;

            if (is_wayland_video_driver())
                return;

            if (!SDL_SetWindowIcon(native, icon_surface))
            {
                TBX_TRACE_WARNING("Failed to set SDL window icon. Error: {}", SDL_GetError());
                SDL_ClearError();
            }
        }
    }

    SdlWindowManager::SdlWindowManager(tbx::IMessageDispatcher& dispatcher)
        : _dispatcher(&dispatcher)
    {
    }

    tbx::Window SdlWindowManager::create(const tbx::WindowCreateInfo& create_info)
    {
        const auto base_name = sanitize_window_handle_name(create_info.title);
        auto handle_name = base_name;
        auto duplicate_index = uint32 {2U};
        auto window = tbx::Handle(handle_name);
        while (_windows.contains(window))
        {
            handle_name = base_name + " (" + std::to_string(duplicate_index) + ")";
            window = tbx::Handle(handle_name);
            ++duplicate_index;
        }

        auto record = SdlWindowRecord {};
        record.id = window;
        record.title = create_info.title;
        record.size = create_info.size;
        record.mode = create_info.mode;
        record.mode_to_restore = create_info.mode == tbx::WindowMode::MINIMIZED
                                     ? tbx::WindowMode::WINDOWED
                                     : create_info.mode;

        _windows[window] = std::move(record);
        TBX_TRACE_INFO(
            "SDL windowing: created managed window {} size={} mode={}.",
            describe_window(window, create_info.title),
            to_string(create_info.size),
            to_string(create_info.mode));

        if (create_info.open_on_creation)
            open(window);

        return window;
    }

    bool SdlWindowManager::destroy(const tbx::Window& window)
    {
        auto* record = try_get_record(window);
        if (!record)
            return false;

        const auto description = describe_window(record->id, record->title);

        if (record->is_open)
            close(window);

        _windows.erase(window);
        auto pending_it = std::ranges::find(_pending_close_window_ids, window);
        if (pending_it != _pending_close_window_ids.end())
            _pending_close_window_ids.erase(pending_it);

        TBX_TRACE_INFO("SDL windowing: destroyed managed window {}.", description);
        return true;
    }

    bool SdlWindowManager::has(const tbx::Window& window) const
    {
        return _windows.contains(window);
    }

    bool SdlWindowManager::open(const tbx::Window& window)
    {
        auto* record = try_get_record(window);
        if (!record)
            return false;

        if (record->is_open)
            return true;

        SDL_Window* native = create_sdl_window(*record);
        if (!native)
            return false;

        record->sdl_window = native;
        record->is_open = true;
        TBX_TRACE_INFO(
            "SDL windowing: opened window {} size={} mode={}.",
            describe_window(record->id, record->title),
            to_string(record->size),
            to_string(record->mode));
        send_native_handle_changed(record->id, nullptr, native);
        send_window_opened(record->id);
        return true;
    }

    bool SdlWindowManager::close(const tbx::Window& window)
    {
        auto* record = try_get_record(window);
        if (!record)
            return false;

        if (!record->is_open)
            return true;

        auto previous_handle = static_cast<tbx::NativeWindowHandle>(record->sdl_window);
        SDL_DestroyWindow(record->sdl_window);
        record->sdl_window = nullptr;
        record->is_open = false;
        TBX_TRACE_INFO(
            "SDL windowing: closed window {}.",
            describe_window(record->id, record->title));
        send_native_handle_changed(record->id, previous_handle, nullptr);
        send_window_closed(record->id);
        return true;
    }

    bool SdlWindowManager::is_open(const tbx::Window& window) const
    {
        const auto* record = try_get_record(window);
        return record ? record->is_open : false;
    }

    tbx::WindowMode SdlWindowManager::get_mode(const tbx::Window& window) const
    {
        const auto* record = try_get_record(window);
        if (!record)
            return tbx::WindowMode::WINDOWED;

        return record->mode;
    }

    bool SdlWindowManager::set_mode(const tbx::Window& window, tbx::WindowMode mode)
    {
        auto* record = try_get_record(window);
        if (!record)
            return false;

        return update_window_mode(*record, mode, true);
    }

    std::string SdlWindowManager::get_title(const tbx::Window& window) const
    {
        const auto* record = try_get_record(window);
        return record ? record->title : std::string {};
    }

    bool SdlWindowManager::set_title(const tbx::Window& window, std::string title)
    {
        auto* record = try_get_record(window);
        if (!record)
            return false;

        if (record->title == title)
            return true;

        const auto previous = record->title;
        record->title = std::move(title);
        if (record->sdl_window)
            SDL_SetWindowTitle(record->sdl_window, record->title.c_str());

#if !defined(TBX_DEBUG)
        TBX_TRACE_INFO(
            "SDL windowing: changed window title for {} from '{}' to '{}'.",
            tbx::to_string(record->id),
            previous,
            record->title);
#endif
        send_window_title_changed(record->id, previous, record->title);
        return true;
    }

    tbx::NativeWindowHandle SdlWindowManager::get_native_handle(const tbx::Window& window) const
    {
        const auto* record = try_get_record(window);
        return record ? static_cast<tbx::NativeWindowHandle>(record->sdl_window) : nullptr;
    }

    tbx::Size SdlWindowManager::get_size(const tbx::Window& window) const
    {
        const auto* record = try_get_record(window);
        return record ? record->size : tbx::Size {};
    }

    bool SdlWindowManager::set_size(const tbx::Window& window, const tbx::Size& size)
    {
        auto* record = try_get_record(window);
        if (!record)
            return false;

        if (are_sizes_equal(record->size, size))
            return true;

        const auto previous = record->size;
        record->size = size;
        if (record->sdl_window)
        {
            SDL_SetWindowSize(
                record->sdl_window,
                static_cast<int>(size.width),
                static_cast<int>(size.height));
        }

        send_window_size_changed(record->id, previous, record->size);
        return true;
    }

    std::vector<tbx::Window> SdlWindowManager::get_open_windows() const
    {
        auto windows = std::vector<tbx::Window> {};
        windows.reserve(_windows.size());

        for (const auto& [window_id, record] : _windows)
        {
            if (record.is_open)
                windows.push_back(window_id);
        }

        return windows;
    }

    void SdlWindowManager::process_event(const SDL_Event& event)
    {
        switch (event.type)
        {
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            {
                auto* window = SDL_GetWindowFromID(event.window.windowID);
                auto* record = try_get_record(window);
                if (record)
                    queue_window_close(record->id);
                break;
            }
            case SDL_EVENT_WINDOW_RESIZED:
            {
                auto* window = SDL_GetWindowFromID(event.window.windowID);
                auto* record = try_get_record(window);
                if (!record)
                    break;

                update_window_size(
                    *record,
                    tbx::Size(
                        static_cast<uint>(event.window.data1),
                        static_cast<uint>(event.window.data2)));
                break;
            }
            case SDL_EVENT_WINDOW_MINIMIZED:
            {
                auto* window = SDL_GetWindowFromID(event.window.windowID);
                auto* record = try_get_record(window);
                if (record)
                    update_window_mode(*record, tbx::WindowMode::MINIMIZED, false);
                break;
            }
            case SDL_EVENT_WINDOW_RESTORED:
            {
                auto* window = SDL_GetWindowFromID(event.window.windowID);
                auto* record = try_get_record(window);
                if (record)
                    update_window_mode(*record, record->mode_to_restore, false);
                break;
            }
            case SDL_EVENT_QUIT:
            {
                for (const auto& [window_id, record] : _windows)
                {
                    if (record.is_open)
                        queue_window_close(window_id);
                }
                break;
            }
            default:
                break;
        }
    }

    void SdlWindowManager::process_pending_window_closes()
    {
        if (_pending_close_window_ids.empty())
            return;

        auto pending = std::move(_pending_close_window_ids);
        _pending_close_window_ids.clear();
        for (const auto& window_id : pending)
            close(window_id);
    }

    void SdlWindowManager::set_icon_surface(SDL_Surface* icon_surface)
    {
        _icon_surface = icon_surface;
        for (const auto& [window_id, record] : _windows)
        {
            (void)window_id;
            try_apply_window_icon(record.sdl_window, _icon_surface);
        }
    }

    void SdlWindowManager::set_use_opengl(bool use_opengl)
    {
        if (_use_opengl == use_opengl)
            return;

        _use_opengl = use_opengl;
        TBX_TRACE_INFO(
            "SDL windowing: switching native window backend to {}.",
            _use_opengl ? "OpenGL" : "SDL");
        recreate_open_windows();
    }

    void SdlWindowManager::shutdown()
    {
        _pending_close_window_ids.clear();
        for (auto& [window_id, record] : _windows)
        {
            (void)window_id;
            if (record.sdl_window)
                SDL_DestroyWindow(record.sdl_window);
            record.sdl_window = nullptr;
            record.is_open = false;
        }
        _windows.clear();
    }

    SDL_Window* SdlWindowManager::create_sdl_window(const SdlWindowRecord& record) const
    {
        uint flags = _use_opengl ? SDL_WINDOW_OPENGL : 0;
        if (record.mode == tbx::WindowMode::BORDERLESS)
            flags |= SDL_WINDOW_BORDERLESS;
        else if (record.mode == tbx::WindowMode::FULLSCREEN)
            flags |= SDL_WINDOW_FULLSCREEN;
        else if (record.mode == tbx::WindowMode::WINDOWED)
            flags |= SDL_WINDOW_RESIZABLE;
        else if (record.mode == tbx::WindowMode::MINIMIZED)
            flags |= SDL_WINDOW_MINIMIZED;

        SDL_Window* native = SDL_CreateWindow(
            record.title.c_str(),
            static_cast<int>(record.size.width),
            static_cast<int>(record.size.height),
            flags);
        if (!native)
        {
            TBX_TRACE_ERROR(
                "SDL windowing: failed to create native window for {}. Error: {}",
                describe_window(record.id, record.title),
                SDL_GetError());
            SDL_ClearError();
            return nullptr;
        }

        try_apply_window_icon(native, _icon_surface);
        return native;
    }

    void SdlWindowManager::recreate_open_windows()
    {
        for (auto& [window_id, record] : _windows)
        {
            if (!record.is_open)
                continue;

            auto previous_handle = static_cast<tbx::NativeWindowHandle>(record.sdl_window);
            SDL_DestroyWindow(record.sdl_window);
            record.sdl_window = create_sdl_window(record);
            if (record.sdl_window)
            {
                TBX_TRACE_INFO(
                    "SDL windowing: recreated window {} for {} backend.",
                    describe_window(window_id, record.title),
                    _use_opengl ? "OpenGL" : "SDL");
                send_native_handle_changed(
                    window_id,
                    previous_handle,
                    static_cast<tbx::NativeWindowHandle>(record.sdl_window));
                continue;
            }

            record.is_open = false;
            TBX_TRACE_WARNING(
                "SDL windowing: failed to recreate window {}; closing it.",
                describe_window(window_id, record.title));
            send_native_handle_changed(window_id, previous_handle, nullptr);
            send_window_closed(window_id);
        }
    }

    void SdlWindowManager::queue_window_close(const tbx::Window& window)
    {
        if (!std::ranges::contains(_pending_close_window_ids, window))
            _pending_close_window_ids.push_back(window);
    }

    bool SdlWindowManager::update_window_mode(
        SdlWindowRecord& record,
        tbx::WindowMode mode,
        bool apply_to_native_window)
    {
        if (record.mode == mode)
            return true;

        const auto previous = record.mode;
        if (mode == tbx::WindowMode::MINIMIZED)
            record.mode_to_restore = previous;
        else
            record.mode_to_restore = mode;

        record.mode = mode;
        if (apply_to_native_window && record.sdl_window)
        {
            if (mode == tbx::WindowMode::BORDERLESS)
            {
                SDL_SetWindowFullscreen(record.sdl_window, false);
                SDL_SetWindowBordered(record.sdl_window, false);
                SDL_RestoreWindow(record.sdl_window);
            }
            else if (mode == tbx::WindowMode::FULLSCREEN)
            {
                SDL_SetWindowBordered(record.sdl_window, true);
                SDL_SetWindowFullscreen(record.sdl_window, true);
            }
            else if (mode == tbx::WindowMode::MINIMIZED)
            {
                SDL_MinimizeWindow(record.sdl_window);
            }
            else if (mode == tbx::WindowMode::WINDOWED)
            {
                SDL_SetWindowFullscreen(record.sdl_window, false);
                SDL_SetWindowBordered(record.sdl_window, true);
                SDL_RestoreWindow(record.sdl_window);
            }
        }

        TBX_TRACE_INFO(
            "SDL windowing: changed window mode for {} from {} to {}.",
            describe_window(record.id, record.title),
            to_string(previous),
            to_string(record.mode));
        send_window_mode_changed(record.id, previous, record.mode);
        return true;
    }

    void SdlWindowManager::update_window_size(SdlWindowRecord& record, const tbx::Size& size)
    {
        if (are_sizes_equal(record.size, size))
            return;

        const auto previous = record.size;
        record.size = size;
        TBX_TRACE_INFO(
            "SDL windowing: resized window {} from {} to {}.",
            describe_window(record.id, record.title),
            to_string(previous),
            to_string(record.size));
        send_window_size_changed(record.id, previous, record.size);
    }

    void SdlWindowManager::send_native_handle_changed(
        const tbx::Window& window,
        tbx::NativeWindowHandle previous_handle,
        tbx::NativeWindowHandle current_handle) const
    {
        if (_dispatcher)
            _dispatcher->send<tbx::WindowNativeHandleChangedEvent>(
                window,
                previous_handle,
                current_handle);
    }

    void SdlWindowManager::send_window_closed(const tbx::Window& window) const
    {
        if (_dispatcher)
            _dispatcher->send<tbx::WindowClosedEvent>(window);
    }

    void SdlWindowManager::send_window_mode_changed(
        const tbx::Window& window,
        tbx::WindowMode previous_mode,
        tbx::WindowMode current_mode) const
    {
        if (_dispatcher)
            _dispatcher->send<tbx::WindowModeChangedEvent>(window, previous_mode, current_mode);
    }

    void SdlWindowManager::send_window_opened(const tbx::Window& window) const
    {
        if (_dispatcher)
            _dispatcher->send<tbx::WindowOpenedEvent>(window);
    }

    void SdlWindowManager::send_window_size_changed(
        const tbx::Window& window,
        const tbx::Size& previous_size,
        const tbx::Size& current_size) const
    {
        if (_dispatcher)
            _dispatcher->send<tbx::WindowSizeChangedEvent>(window, previous_size, current_size);
    }

    void SdlWindowManager::send_window_title_changed(
        const tbx::Window& window,
        const std::string& previous_title,
        const std::string& current_title) const
    {
        if (_dispatcher)
            _dispatcher->send<tbx::WindowTitleChangedEvent>(window, previous_title, current_title);
    }

    const SdlWindowRecord* SdlWindowManager::try_get_record(const tbx::Window& window) const
    {
        const auto it = _windows.find(window);
        if (it == _windows.end())
            return nullptr;

        return &it->second;
    }

    SdlWindowRecord* SdlWindowManager::try_get_record(const tbx::Window& window)
    {
        const auto* self = static_cast<const SdlWindowManager*>(this);
        return const_cast<SdlWindowRecord*>(self->try_get_record(window));
    }

    SdlWindowRecord* SdlWindowManager::try_get_record(const SDL_Window* sdl_window)
    {
        const auto it = std::ranges::find_if(
            _windows,
            [sdl_window](const auto& entry)
            {
                return entry.second.sdl_window == sdl_window;
            });
        if (it == _windows.end())
            return nullptr;

        return &it->second;
    }
}
