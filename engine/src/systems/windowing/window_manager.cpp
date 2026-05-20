#include "tbx/systems/windowing/window_manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/messages.h"
#include "tbx/systems/windowing/internal/window_manager_internal.h"
#include <algorithm>
#include <string_view>
#include <utility>

namespace tbx
{
    WindowManager::WindowManager(IMessageDispatcher& dispatcher, IWindowBackend& backend)
        : _dispatcher(dispatcher)
        , _backend(backend)
    {
    }

    WindowManager::~WindowManager() noexcept
    {
        shutdown();
    }

    Window WindowManager::open(const WindowCreateInfo& create_info)
    {
        const auto base_name = internal::sanitize_window_handle_name(create_info.title);
        auto handle_name = base_name;
        auto duplicate_index = uint32 {2U};
        auto window = Handle(handle_name);
        while (_windows.contains(window))
        {
            handle_name = base_name + " (" + std::to_string(duplicate_index) + ")";
            window = Handle(handle_name);
            ++duplicate_index;
        }

        auto record = ManagedWindowRecord {};
        record.id = window;
        record.title = create_info.title;
        record.size = create_info.size;
        record.mode = create_info.mode;
        record.mode_to_restore =
            create_info.mode == WindowMode::MINIMIZED ? WindowMode::WINDOWED : create_info.mode;
        record.is_open = true;

        auto native_handle = NativeWindowHandle {nullptr};
        if (!_backend.create_window(window, create_info, native_handle))
            return {};

        if (!native_handle)
        {
            TBX_TRACE_ERROR(
                "Window manager: backend returned a null native handle for window '{}'.",
                to_string(window));
            _backend.destroy_window(window);
            return {};
        }

        record.native_handle = native_handle;
        _windows[window] = std::move(record);
        if (!_main_window.is_valid())
            _main_window = window;
        send_window_opened(window);
        return window;
    }

    bool WindowManager::close(const Window& window)
    {
        auto* record = try_get_record(window);
        if (!record)
            return false;

        if (!_backend.destroy_window(window))
            return false;

        const bool was_main_window = (_main_window == window);
        const Window closed_window = record->id;
        if (was_main_window)
            _main_window = {};

        _windows.erase(window);
        auto pending_it = std::ranges::find(_pending_close_window_ids, window);
        if (pending_it != _pending_close_window_ids.end())
            _pending_close_window_ids.erase(pending_it);
        send_window_closed(closed_window);
        return true;
    }

    bool WindowManager::has(const Window& window) const
    {
        return _windows.contains(window);
    }

    bool WindowManager::is_open(const Window& window) const
    {
        const auto* record = try_get_record(window);
        return record != nullptr;
    }

    WindowMode WindowManager::get_mode(const Window& window) const
    {
        const auto* record = try_get_record(window);
        if (!record)
            return WindowMode::WINDOWED;

        return record->mode;
    }

    bool WindowManager::set_mode(const Window& window, WindowMode mode)
    {
        auto* record = try_get_record(window);
        if (!record)
            return false;

        return update_window_mode(*record, mode, true);
    }

    std::string WindowManager::get_title(const Window& window) const
    {
        const auto* record = try_get_record(window);
        return record ? record->title : std::string {};
    }

    bool WindowManager::set_title(const Window& window, std::string title)
    {
        auto* record = try_get_record(window);
        if (!record)
            return false;
        if (record->title == title)
            return true;

        if (record->is_open)
        {
            if (!_backend.set_window_title(window, title))
                return false;
        }

        const auto previous_title = record->title;
        record->title = std::move(title);
        send_window_title_changed(record->id, previous_title, record->title);
        return true;
    }

    NativeWindowHandle WindowManager::get_native_handle(const Window& window) const
    {
        const auto* record = try_get_record(window);
        return record ? record->native_handle : nullptr;
    }

    Size WindowManager::get_size(const Window& window) const
    {
        const auto* record = try_get_record(window);
        return record ? record->size : Size {};
    }

    bool WindowManager::set_size(const Window& window, const Size& size)
    {
        auto* record = try_get_record(window);
        if (!record)
            return false;

        return update_window_size(*record, size, true);
    }

    std::vector<Window> WindowManager::get_open_windows() const
    {
        auto windows = std::vector<Window> {};
        windows.reserve(_windows.size());
        for (const auto& [window_id, record] : _windows)
            windows.push_back(window_id);

        return windows;
    }

    bool WindowManager::has_main_window() const
    {
        return _main_window.is_valid() && has(_main_window);
    }

    const Window& WindowManager::get_main_window() const
    {
        return _main_window;
    }

    bool WindowManager::set_main_window(const Window& window)
    {
        if (!has(window))
            return false;

        _main_window = window;
        return true;
    }

    void WindowManager::update()
    {
        auto events = std::vector<WindowBackendEvent> {};
        _backend.pump_events(events);

        for (const auto& event : events)
            handle_backend_event(event);

        process_pending_window_closes();
    }

    void WindowManager::shutdown()
    {
        for (const auto& [window_id, record] : _windows)
        {
            (void)record;
            queue_window_close(window_id);
        }
        process_pending_window_closes();

        _backend.shutdown();

        _pending_close_window_ids.clear();
        _windows.clear();
        _main_window = {};
    }

    void WindowManager::handle_backend_event(const WindowBackendEvent& event)
    {
        switch (event.type)
        {
            case WindowBackendEventType::CLOSE_REQUESTED:
                if (has(event.window))
                    queue_window_close(event.window);
                break;

            case WindowBackendEventType::RESIZED:
            {
                auto* record = try_get_record(event.window);
                if (record)
                    update_window_size(*record, event.size, false);
                break;
            }

            case WindowBackendEventType::MINIMIZED:
            {
                auto* record = try_get_record(event.window);
                if (record)
                    update_window_mode(*record, WindowMode::MINIMIZED, false);
                break;
            }

            case WindowBackendEventType::RESTORED:
            {
                auto* record = try_get_record(event.window);
                if (record)
                    update_window_mode(*record, record->mode_to_restore, false);
                break;
            }

            case WindowBackendEventType::QUIT_REQUESTED:
                for (const auto& [window_id, record] : _windows)
                {
                    (void)record;
                    queue_window_close(window_id);
                }
                break;
        }
    }

    void WindowManager::process_pending_window_closes()
    {
        if (_pending_close_window_ids.empty())
            return;

        auto pending_windows = std::move(_pending_close_window_ids);
        _pending_close_window_ids.clear();
        for (const auto& window : pending_windows)
            close(window);
    }

    void WindowManager::queue_window_close(const Window& window)
    {
        if (!std::ranges::contains(_pending_close_window_ids, window))
            _pending_close_window_ids.push_back(window);
    }

    bool WindowManager::update_window_mode(
        ManagedWindowRecord& record,
        WindowMode mode,
        bool apply_to_native_window)
    {
        if (record.mode == mode)
            return true;

        if (apply_to_native_window)
        {
            if (!_backend.set_window_mode(record.id, mode))
                return false;
        }

        const auto previous_mode = record.mode;
        if (mode == WindowMode::MINIMIZED)
            record.mode_to_restore = previous_mode;
        else
            record.mode_to_restore = mode;
        record.mode = mode;

        send_window_mode_changed(record.id, previous_mode, record.mode);
        return true;
    }

    bool WindowManager::update_window_size(
        ManagedWindowRecord& record,
        const Size& size,
        bool apply_to_native_window)
    {
        if (internal::are_sizes_equal(record.size, size))
            return true;

        if (apply_to_native_window)
        {
            if (!_backend.set_window_size(record.id, size))
                return false;
        }

        const auto previous_size = record.size;
        record.size = size;
        send_window_size_changed(record.id, previous_size, record.size);
        return true;
    }

    void WindowManager::send_window_closed(const Window& window) const
    {
        _dispatcher.send<WindowClosedEvent>(window);
    }

    void WindowManager::send_window_mode_changed(
        const Window& window,
        WindowMode previous_mode,
        WindowMode current_mode) const
    {
        _dispatcher.send<WindowModeChangedEvent>(window, previous_mode, current_mode);
    }

    void WindowManager::send_window_opened(const Window& window) const
    {
        _dispatcher.send<WindowOpenedEvent>(window);
    }

    void WindowManager::send_window_size_changed(
        const Window& window,
        const Size& previous_size,
        const Size& current_size) const
    {
        _dispatcher.send<WindowSizeChangedEvent>(window, previous_size, current_size);
    }

    void WindowManager::send_window_title_changed(
        const Window& window,
        const std::string& previous_title,
        const std::string& current_title) const
    {
        _dispatcher.send<WindowTitleChangedEvent>(window, previous_title, current_title);
    }

    const ManagedWindowRecord* WindowManager::try_get_record(const Window& window) const
    {
        const auto it = _windows.find(window);
        if (it == _windows.end())
            return nullptr;

        return &it->second;
    }

    ManagedWindowRecord* WindowManager::try_get_record(const Window& window)
    {
        const auto* self = static_cast<const WindowManager*>(this);
        return const_cast<ManagedWindowRecord*>(self->try_get_record(window));
    }
}
