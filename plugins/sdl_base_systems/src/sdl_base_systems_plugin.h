#pragma once
#include "tbx/plugin_api/plugin.h"
#include <SDL3/SDL_log.h>

namespace sdl_base_systems
{
    /// <summary>Initializes shared SDL subsystems used by other plugins.</summary>
    /// <remarks>Purpose: Owns SDL initialization for core SDL subsystems used engine-wide.
    /// Ownership: Owns the SDL subsystems it initializes and shuts them down on detach.
    /// Thread Safety: Expected to be used on the main thread.</remarks>
    class SdlBaseSystemsPlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::IPluginHost& host) override;
        void on_detach() override;
        void on_update(const tbx::DeltaTime& dt) override;

      private:
        bool _owns_sdl = false;
    };
}
