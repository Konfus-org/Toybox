#pragma once
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/interfaces/window_backend.h"
#include "tbx/interfaces/window_manager.h"

namespace tbx
{
    /// @brief
    /// Purpose: Application-owned service that tracks windows and routes operations to a backend.
    /// @details
    /// Ownership: Owns managed window metadata and borrows the active window backend.
    /// Thread Safety: Not thread-safe; call from the application main thread.
    class TBX_API WindowManager final : public IWindowManager
    {
      public:
        WindowManager(
            std::weak_ptr<IMessageDispatcher> dispatcher,
            std::weak_ptr<IWindowBackend> backend);
        ~WindowManager() noexcept override;

      public:
        WindowManager(const WindowManager&) = delete;
        WindowManager& operator=(const WindowManager&) = delete;
        WindowManager(WindowManager&&) noexcept = delete;
        WindowManager& operator=(WindowManager&&) noexcept = delete;

      public:
        Window open(const WindowCreateInfo& create_info = {}) override;
        bool close(const Window& window) override;
        bool has(const Window& window) const override;
        bool is_open(const Window& window) const override;
        WindowMode get_mode(const Window& window) const override;
        bool set_mode(const Window& window, WindowMode mode) override;
        std::string get_title(const Window& window) const override;
        bool set_title(const Window& window, std::string title) override;
        NativeWindowHandle get_native_handle(const Window& window) const override;
        Size get_size(const Window& window) const override;
        bool set_size(const Window& window, const Size& size) override;
        std::vector<Window> get_open_windows() const override;
        bool has_main_window() const override;
        const Window& get_main_window() const override;
        bool set_main_window(const Window& window) override;
        void update() override;
        void shutdown() override;

      private:
        struct ManagedWindowRecord;
        struct State;

      private:
        void handle_backend_event(const WindowBackendEvent& event);
        void process_pending_window_closes();
        void queue_window_close(const Window& window);
        bool update_window_mode(
            ManagedWindowRecord& record,
            WindowMode mode,
            bool apply_to_native_window);
        bool update_window_size(
            ManagedWindowRecord& record,
            const Size& size,
            bool apply_to_native_window);
        void send_window_closed(const Window& window) const;
        void send_window_mode_changed(
            const Window& window,
            WindowMode previous_mode,
            WindowMode current_mode) const;
        void send_window_opened(const Window& window) const;
        void send_window_size_changed(
            const Window& window,
            const Size& previous_size,
            const Size& current_size) const;
        void send_window_title_changed(
            const Window& window,
            const std::string& previous_title,
            const std::string& current_title) const;
        const ManagedWindowRecord* try_get_record(const Window& window) const;
        ManagedWindowRecord* try_get_record(const Window& window);

      private:
        std::weak_ptr<IMessageDispatcher> _dispatcher = {};
        std::weak_ptr<IWindowBackend> _backend = {};
        std::unique_ptr<State> _state = {};
    };
}
