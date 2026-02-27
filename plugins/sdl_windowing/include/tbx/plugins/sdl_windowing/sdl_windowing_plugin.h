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
    using namespace tbx;
    struct TBX_PLUGIN_API SdlWindowRecord
    {
        SDL_Window* sdl_window = nullptr;
        Window* tbx_window = nullptr;
        WindowMode mode_to_restore = WindowMode::WINDOWED;
    };

    class TBX_PLUGIN_API SdlWindowingPlugin final : public Plugin
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
        void process_pending_window_closes();
        SdlWindowRecord* try_get_record(std::function<bool(const SdlWindowRecord&)> condition);
        SdlWindowRecord* try_get_record(const SDL_Window* sdl_window);
        SdlWindowRecord* try_get_record(const Window* tbx_window);
        SdlWindowRecord* try_get_record(const Uuid& window_id);
        SdlWindowRecord& add_record(SDL_Window* sdl_window, Window* tbx_window);
        void remove_record(const SdlWindowRecord& record);

      private:
        std::vector<SdlWindowRecord> _windows;
        std::vector<Uuid> _pending_close_window_ids;
        bool _use_opengl = false;
        SDL_Surface* _window_icon_surface = nullptr;
    };
}
