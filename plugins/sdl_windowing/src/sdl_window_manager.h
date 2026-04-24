#pragma once
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/interfaces/window_manager.h"
#include <SDL3/SDL.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace sdl_windowing
{
    struct SdlWindowRecord
    {
        tbx::Window id = {};
        std::string title = "Toybox";
        tbx::Size size = {1280, 720};
        tbx::WindowMode mode = tbx::WindowMode::WINDOWED;
        tbx::WindowMode mode_to_restore = tbx::WindowMode::WINDOWED;
        bool is_open = false;
        SDL_Window* sdl_window = nullptr;
    };

    class SdlWindowManager final : public tbx::IWindowManager
    {
      public:
        SdlWindowManager(tbx::IMessageDispatcher& dispatcher);

        tbx::Window create(const tbx::WindowCreateInfo& create_info) override;
        bool destroy(const tbx::Window& window) override;
        bool has(const tbx::Window& window) const override;
        bool open(const tbx::Window& window) override;
        bool close(const tbx::Window& window) override;
        bool is_open(const tbx::Window& window) const override;
        tbx::WindowMode get_mode(const tbx::Window& window) const override;
        bool set_mode(const tbx::Window& window, tbx::WindowMode mode) override;
        std::string get_title(const tbx::Window& window) const override;
        bool set_title(const tbx::Window& window, std::string title) override;
        tbx::NativeWindowHandle get_native_handle(const tbx::Window& window) const override;
        tbx::Size get_size(const tbx::Window& window) const override;
        bool set_size(const tbx::Window& window, const tbx::Size& size) override;
        std::vector<tbx::Window> get_open_windows() const override;

        void process_event(const SDL_Event& event);
        void process_pending_window_closes();
        void set_icon_surface(SDL_Surface* icon_surface);
        void set_use_opengl(bool use_opengl);
        void shutdown();

      private:
        SDL_Window* create_sdl_window(const SdlWindowRecord& record) const;
        void recreate_open_windows();
        void queue_window_close(const tbx::Window& window);
        bool update_window_mode(
            SdlWindowRecord& record,
            tbx::WindowMode mode,
            bool apply_to_native_window);
        void update_window_size(SdlWindowRecord& record, const tbx::Size& size);
        void send_native_handle_changed(
            const tbx::Window& window,
            tbx::NativeWindowHandle previous_handle,
            tbx::NativeWindowHandle current_handle) const;
        void send_window_closed(const tbx::Window& window) const;
        void send_window_mode_changed(
            const tbx::Window& window,
            tbx::WindowMode previous_mode,
            tbx::WindowMode current_mode) const;
        void send_window_opened(const tbx::Window& window) const;
        void send_window_size_changed(
            const tbx::Window& window,
            const tbx::Size& previous_size,
            const tbx::Size& current_size) const;
        void send_window_title_changed(
            const tbx::Window& window,
            const std::string& previous_title,
            const std::string& current_title) const;
        const SdlWindowRecord* try_get_record(const tbx::Window& window) const;
        SdlWindowRecord* try_get_record(const tbx::Window& window);
        SdlWindowRecord* try_get_record(const SDL_Window* sdl_window);

      private:
        tbx::IMessageDispatcher& _dispatcher;
        std::unordered_map<tbx::Window, SdlWindowRecord> _windows = {};
        std::vector<tbx::Window> _pending_close_window_ids = {};
        SDL_Surface* _icon_surface = nullptr;
        bool _use_opengl = false;
    };
}
