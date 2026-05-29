#include "threed_example.h"
#include "scripts/flashlight_controller.h"
#include "scripts/player_controller.h"
#include "scripts/sky_rotator.h"
#include "tbx/systems/assets/serialization.h"

namespace three_d_example
{
    void ThreeDExampleRuntimePlugin::on_attach(tbx::ServiceProvider&)
    {
        // TODO: Make it to where we don't need to manually register
        static_cast<void>(tbx::register_script_asset_type<PlayerController>(
            1U,
            tbx_apply_script_overrides_PlayerController,
            tbx_bind_script_runtime_PlayerController));
        static_cast<void>(tbx::register_script_asset_type<FlashlightController>(
            1U,
            tbx_apply_script_overrides_FlashlightController,
            tbx_bind_script_runtime_FlashlightController));
        static_cast<void>(tbx::register_script_asset_type<SkyRotator>(
            1U,
            tbx_apply_script_overrides_SkyRotator,
            tbx_bind_script_runtime_SkyRotator));
    }
}
