#pragma once
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <SDL3/SDL_log.h>

namespace sdl_base_systems
{
    /// @brief
    /// Purpose: Owns SDL initialization for core SDL subsystems used engine-wide.
    /// @details
    /// Ownership: Owns the SDL subsystems it initializes and shuts them down on detach.
    /// Thread Safety: Expected to be used on the main thread.
    [[tbx::plugin(
        name = "SdlBaseSystems",
        version = "1.0.0",
        category = tbx::PluginCategory::INPUT)]];
    class TBX_PLUGIN_API SdlBaseSystems final : public tbx::Plugin
    {
      protected:
        void on_attach() override;
        void on_detach() override;
        void on_update(const tbx::DeltaTime& dt) override;
    };
}
