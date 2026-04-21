#pragma once
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_export.h"
#include "tbx/plugin_api/service_provider.h"
#include <SDL3/SDL.h>

namespace sdl_windowing
{
    class SdlWindowManager;

    class TBX_PLUGIN_API SdlWindowingPlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach() override;
        void on_update(const tbx::DeltaTime& dt) override;
        void on_recieve_message(tbx::Message& msg) override;

      private:
        tbx::ServiceProvider* _service_provider = nullptr;
        SdlWindowManager* _window_manager = nullptr;
        bool _use_opengl = false;
        SDL_Surface* _window_icon_surface = nullptr;
    };
}
