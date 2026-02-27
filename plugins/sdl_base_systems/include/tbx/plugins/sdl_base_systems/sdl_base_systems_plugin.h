#pragma once
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_export.h"
#include <SDL3/SDL_log.h>

#ifndef TBX_SDL_BASE_SYSTEMS_PLUGIN_EXPORTS
    #define TBX_SDL_BASE_SYSTEMS_PLUGIN_EXPORTS 0
#endif

namespace sdl_base_systems
{
    using namespace tbx;
    /// <summary>Initializes shared SDL subsystems used by other plugins.</summary>
    /// <remarks>Purpose: Owns SDL initialization for core SDL subsystems used engine-wide.
    /// Ownership: Owns the SDL subsystems it initializes and shuts them down on detach.
    /// Thread Safety: Expected to be used on the main thread.</remarks>
    class TBX_PLUGIN_INCLUDE_API(TBX_SDL_BASE_SYSTEMS_PLUGIN_EXPORTS) SdlBaseSystemsPlugin final : public Plugin
    {
      public:
        void on_attach(IPluginHost& host) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;

      private:
        bool _owns_sdl = false;
    };
}
