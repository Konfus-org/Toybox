#pragma once
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include <SDL3/SDL.h>

namespace sdl_windowing
{
    class SdlWindowBackend;

    [[tbx::plugin]];
    [[tbx::name("SdlWindowingPlugin")]];
    [[tbx::version("1.0.0")]];
    [[tbx::category("input")]];
    class TBX_PLUGIN_API SdlWindowingPlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach(tbx::ServiceProvider& service_provider) override;
        void on_update(const tbx::DeltaTime& dt) override;
        void on_recieve_message(tbx::Message& msg) override;

      private:
        std::weak_ptr<SdlWindowBackend> _window_backend = {};
        bool _use_opengl = false;
        SDL_Surface* _window_icon_surface = nullptr;
    };
}
