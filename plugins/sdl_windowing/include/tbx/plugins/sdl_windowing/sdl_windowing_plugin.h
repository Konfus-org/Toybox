#pragma once
#include "tbx/graphics/messages.h"
#include "tbx/graphics/window.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_export.h"
#include <SDL3/SDL.h>
#include <functional>
#include <string>
#include <vector>


namespace sdl_windowing
{
    struct TBX_PLUGIN_API SdlWindowRecord
    {
        SDL_Window* sdl_window = nullptr;
        tbx::Window* tbx_window = nullptr;
        tbx::WindowMode mode_to_restore = tbx::WindowMode::WINDOWED;
    };

    class TBX_PLUGIN_API SdlWindowingPlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::IPluginHost& host) override;
        void on_detach() override;
        void on_update(const tbx::DeltaTime& dt) override;
        void on_recieve_message(tbx::Message& msg) override;

      private:
        void on_window_is_open_changed(tbx::PropertyChangedEvent<tbx::Window, bool>& event);
        void on_window_size_changed(tbx::PropertyChangedEvent<tbx::Window, tbx::Size>& event);
        void on_window_mode_changed(tbx::PropertyChangedEvent<tbx::Window, tbx::WindowMode>& event);
        void on_window_title_changed(tbx::PropertyChangedEvent<tbx::Window, std::string>& event);
        void process_pending_window_closes();
        SdlWindowRecord* try_get_record(std::function<bool(const SdlWindowRecord&)> condition);
        SdlWindowRecord* try_get_record(const SDL_Window* sdl_window);
        SdlWindowRecord* try_get_record(const tbx::Window* tbx_window);
        SdlWindowRecord* try_get_record(const tbx::Uuid& window_id);
        SdlWindowRecord& add_record(SDL_Window* sdl_window, tbx::Window* tbx_window);
        void remove_record(const SdlWindowRecord& record);

      private:
        std::vector<SdlWindowRecord> _windows;
        std::vector<tbx::Uuid> _pending_close_window_ids;
        bool _use_opengl = false;
        SDL_Surface* _window_icon_surface = nullptr;
    };
}
