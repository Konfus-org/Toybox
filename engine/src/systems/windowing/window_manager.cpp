#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/messages.h"
#include "tbx/systems/windowing/manager.h"

namespace tbx
{
    static bool are_sizes_equal(const Size& left, const Size& right)
    {
        return left.width == right.width && left.height == right.height;
    }

    static std::string sanitize_window_handle_name(std::string title)
    {
        if (title.empty())
            return "Toybox";

        return title;
    }

    struct WindowManager::ManagedWindowRecord
    {
        Window id = {};
        std::string title = "Toybox";
        Size size = {1280, 720};
        WindowMode mode = WindowMode::WINDOWED;
        WindowMode mode_to_restore = WindowMode::WINDOWED;
        bool is_open = false;
    };

    struct WindowManager::State
    {
        std::unordered_map<Window, ManagedWindowRecord> windows = {};
        std::vector<Window> pending_close_window_ids = {};
        Window main_window = {};
    };

    WindowManager::WindowManager(
        std::weak_ptr<IMessageDispatcher> dispatcher,
        std::weak_ptr<IWindowBackend> backend)
        : _dispatcher(std::move(dispatcher))
        , _backend(std::move(backend))
        , _state(std::make_unique<State>())
    {
    }

    WindowManager::~WindowManager() noexcept
    {
        shutdown();
    }

    Window WindowManager::open(const WindowCreateInfo& create_info)
    {
        const auto base_name = sanitize_window_handle_name(create_info.title);
        auto handle_name = base_name;
        auto duplicate_index = uint32 {2U};
        auto window = Window(handle_name);
        while (_state->windows.contains(window))
        {
            handle_name = base_name + " (" + std::to_string(duplicate_index) + ")";
            window = Window(handle_name);
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
        const auto backend = _backend.lock();
        if (!backend)
            return {};

        if (!backend->create_window(window, create_info, native_handle))
            return {};

        if (!native_handle)
        {
            TBX_TRACE_ERROR(
                "Window manager: backend returned a null native handle for window '{}'.",
                window);
            backend->destroy_window(window);
            return {};
        }

        window.native_handle = native_handle;
        record.id = window;
        _state->windows[window] = std::move(record);
        if (!_state->main_window.id.is_valid())
            _state->main_window = window;
        send_window_opened(window);
        return window;
    }

    bool WindowManager::close(const Window& window)
    {
        auto* record = try_get_record(window);
        if (!record)
            return false;

        const auto backend = _backend.lock();
        if (!backend || !backend->destroy_window(window))
            return false;

        const bool was_main_window = (_state->main_window == window);
        const Window closed_window = record->id;
        record->id.invalidate();
        if (was_main_window)
            _state->main_window = {};

        _state->windows.erase(window);
        auto pending_it = std::ranges::find(_state->pending_close_window_ids, window);
        if (pending_it != _state->pending_close_window_ids.end())
            _state->pending_close_window_ids.erase(pending_it);
        send_window_closed(closed_window);
        return true;
    }

    bool WindowManager::has(const Window& window) const
    {
        return _state->windows.contains(window);
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
            const auto backend = _backend.lock();
            if (!backend || !backend->set_window_title(window, title))
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
        return record ? record->id.native_handle : nullptr;
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
        windows.reserve(_state->windows.size());
        for (const auto& [window_id, record] : _state->windows)
            windows.push_back(window_id);

        return windows;
    }

    bool WindowManager::has_main_window() const
    {
        return _state->main_window.id.is_valid() && has(_state->main_window);
    }

    const Window& WindowManager::get_main_window() const
    {
        return _state->main_window;
    }

    bool WindowManager::set_main_window(const Window& window)
    {
        if (!has(window))
            return false;

        _state->main_window = window;
        return true;
    }

    void WindowManager::update()
    {
        auto events = std::vector<WindowBackendEvent> {};
        const auto backend = _backend.lock();
        if (!backend)
            return;

        backend->pump_events(events);

        for (const auto& event : events)
            handle_backend_event(event);

        process_pending_window_closes();
    }

    void WindowManager::shutdown()
    {
        for (const auto& [window_id, record] : _state->windows)
        {
            (void)record;
            queue_window_close(window_id);
        }
        process_pending_window_closes();

        if (const auto backend = _backend.lock())
            backend->shutdown();

        _state->pending_close_window_ids.clear();
        _state->windows.clear();
        _state->main_window = {};
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
                for (const auto& [window_id, record] : _state->windows)
                {
                    (void)record;
                    queue_window_close(window_id);
                }
                break;
        }
    }

    void WindowManager::process_pending_window_closes()
    {
        if (_state->pending_close_window_ids.empty())
            return;

        auto pending_windows = std::move(_state->pending_close_window_ids);
        _state->pending_close_window_ids.clear();
        for (const auto& window : pending_windows)
            close(window);
    }

    void WindowManager::queue_window_close(const Window& window)
    {
        if (!std::ranges::contains(_state->pending_close_window_ids, window))
            _state->pending_close_window_ids.push_back(window);
    }

    bool WindowManager::update_window_mode(
        WindowManager::ManagedWindowRecord& record,
        WindowMode mode,
        bool apply_to_native_window)
    {
        if (record.mode == mode)
            return true;

        if (apply_to_native_window)
        {
            const auto backend = _backend.lock();
            if (!backend || !backend->set_window_mode(record.id, mode))
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
        WindowManager::ManagedWindowRecord& record,
        const Size& size,
        bool apply_to_native_window)
    {
        if (are_sizes_equal(record.size, size))
            return true;

        if (apply_to_native_window)
        {
            const auto backend = _backend.lock();
            if (!backend || !backend->set_window_size(record.id, size))
                return false;
        }

        const auto previous_size = record.size;
        record.size = size;
        send_window_size_changed(record.id, previous_size, record.size);
        return true;
    }

    void WindowManager::send_window_closed(const Window& window) const
    {
        if (const auto dispatcher = _dispatcher.lock())
            dispatcher->send<WindowClosedEvent>(window);
    }

    void WindowManager::send_window_mode_changed(
        const Window& window,
        WindowMode previous_mode,
        WindowMode current_mode) const
    {
        if (const auto dispatcher = _dispatcher.lock())
            dispatcher->send<WindowModeChangedEvent>(window, previous_mode, current_mode);
    }

    void WindowManager::send_window_opened(const Window& window) const
    {
        if (const auto dispatcher = _dispatcher.lock())
            dispatcher->send<WindowOpenedEvent>(window);
    }

    void WindowManager::send_window_size_changed(
        const Window& window,
        const Size& previous_size,
        const Size& current_size) const
    {
        if (const auto dispatcher = _dispatcher.lock())
            dispatcher->send<WindowSizeChangedEvent>(window, previous_size, current_size);
    }

    void WindowManager::send_window_title_changed(
        const Window& window,
        const std::string& previous_title,
        const std::string& current_title) const
    {
        if (const auto dispatcher = _dispatcher.lock())
            dispatcher->send<WindowTitleChangedEvent>(window, previous_title, current_title);
    }

    const WindowManager::ManagedWindowRecord* WindowManager::try_get_record(
        const Window& window) const
    {
        const auto it = _state->windows.find(window);
        if (it == _state->windows.end())
            return nullptr;

        return &it->second;
    }

    WindowManager::ManagedWindowRecord* WindowManager::try_get_record(const Window& window)
    {
        const auto* self = static_cast<const WindowManager*>(this);
        return const_cast<WindowManager::ManagedWindowRecord*>(self->try_get_record(window));
    }
}
