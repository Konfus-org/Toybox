#pragma once
#include "tbx/graphics/window.h"
#include "tbx/plugin_api/plugin.h"
#include <SDL3/SDL.h>
#include <vector>

namespace tbx::plugins
{
    struct SdlWindowRecord
    {
        SDL_Window* sdl_window = nullptr;
        Window* tbx_window = nullptr;
    };

    class SdlWindowingPlugin final : public Plugin
    {
      public:
        void on_attach(Application& host) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;
        void on_recieve_message(Message& msg) override;

      private:
        void on_window_is_open_changed(PropertyChangedEvent<Window, bool>& event);
        void on_window_size_changed(PropertyChangedEvent<Window, Size>& event);
        void on_window_mode_changed(PropertyChangedEvent<Window, WindowMode>& event);
        void on_window_title_changed(PropertyChangedEvent<Window, String>& event);
        SdlWindowRecord find_record(std::function<bool(const SdlWindowRecord&)> condition);
        SdlWindowRecord find_record(const SDL_Window* sdl_window);
        SdlWindowRecord find_record(const Window* tbx_window);
        SdlWindowRecord& add_record(SDL_Window* sdl_window, Window* tbx_window);
        void remove_record(const SdlWindowRecord& record);

      private:
        std::vector<SdlWindowRecord> _windows;
        bool _use_opengl = false;
    };
}
