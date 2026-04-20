#pragma once
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_export.h"
#include <SDL3/SDL_log.h>

namespace sdl_base_systems
{
    /// @brief
    /// Purpose: Owns SDL initialization for core SDL subsystems used engine-wide.
    /// @details
    /// Ownership: Owns the SDL subsystems it initializes and shuts them down on detach.
    /// Thread Safety: Expected to be used on the main thread.
    class TBX_PLUGIN_API SdlBaseSystemsPlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach() override;
        void on_update(const tbx::DeltaTime& dt) override;

      private:
        bool _owns_sdl = false;
    };
}
