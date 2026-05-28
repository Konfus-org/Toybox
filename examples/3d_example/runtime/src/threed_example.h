#pragma once
#include "player.h"
#include "sky_system.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/handle.h"
#include <memory>

namespace three_d_example
{
    /// @brief
    /// Purpose: Loads the authored 3D example world and wires its runtime gameplay systems.
    [[tbx::plugin]];
    [[tbx::name("ThreeDExampleRuntime")]];
    [[tbx::version("1.0.0")]];
    [[tbx::category("gameplay")]];
    class ThreeDExampleRuntimePlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach(tbx::ServiceProvider& service_provider) override;
        void on_update(const tbx::DeltaTime& dt) override;

      private:
        std::weak_ptr<tbx::AssetManager> _asset_manager = {};
        std::unique_ptr<Player> _player = nullptr;
        SkySystem _sky_system = {};
        tbx::Handle _world_handle = {};
    };
}
