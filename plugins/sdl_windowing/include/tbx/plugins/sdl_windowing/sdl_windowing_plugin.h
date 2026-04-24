#pragma once
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include <SDL3/SDL.h>
#include <functional>
#include <optional>

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
        std::optional<std::reference_wrapper<tbx::ServiceProvider>> _service_provider =
            std::nullopt;
        std::optional<std::reference_wrapper<SdlWindowManager>> _window_manager = std::nullopt;
        bool _use_opengl = false;
        SDL_Surface* _window_icon_surface = nullptr;
    };
}
