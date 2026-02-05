#pragma once
#include "tbx/graphics/messages.h"
#include "tbx/graphics/window.h"
#include "tbx/plugin_api/plugin.h"
#include <SDL3/SDL.h>
#include <functional>
#include <string>
#include <vector>

namespace tbx::plugins
{
    struct SdlWindowRecord
    {
        SDL_Window* sdl_window = nullptr;
        SDL_GLContext gl_context = nullptr;
        Window* tbx_window = nullptr;
        WindowMode last_window_mode = WindowMode::WINDOWED;
    };

    class SdlWindowingPlugin final : public Plugin
    {
      public:
        void on_attach(IPluginHost& host) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;
        void on_recieve_message(Message& msg) override;

      private:
        void on_window_is_open_changed(PropertyChangedEvent<Window, bool>& event);
        void on_window_size_changed(PropertyChangedEvent<Window, Size>& event);
        void on_window_mode_changed(PropertyChangedEvent<Window, WindowMode>& event);
        void on_window_title_changed(PropertyChangedEvent<Window, std::string>& event);
        void on_window_make_current(WindowMakeCurrentRequest& request);
        void on_window_present(WindowPresentRequest& request);
        SDL_GLContext create_gl_context(SDL_Window* sdl_window, Window* tbx_window);
        void destroy_gl_context(SdlWindowRecord& record);
        SdlWindowRecord* try_get_record(std::function<bool(const SdlWindowRecord&)> condition);
        SdlWindowRecord* try_get_record(const SDL_Window* sdl_window);
        SdlWindowRecord* try_get_record(const Window* tbx_window);
        SdlWindowRecord* try_get_record(const Uuid& window_id);
        SdlWindowRecord& add_record(SDL_Window* sdl_window, Window* tbx_window);
        void remove_record(const SdlWindowRecord& record);

      private:
        std::vector<SdlWindowRecord> _windows;
        bool _use_opengl = false;
    };
}
