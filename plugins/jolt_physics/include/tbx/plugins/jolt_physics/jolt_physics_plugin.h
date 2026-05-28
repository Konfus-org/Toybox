#pragma once
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/plugin_api/plugin_export.h"

namespace jolt_physics
{
    [[tbx::plugin]];
    [[tbx::name("JoltPhysicsPlugin")]];
    [[tbx::version("1.0.0")]];
    [[tbx::category("physics")]];
    [[tbx::priority(50)]];
    class TBX_PLUGIN_API JoltPhysicsPlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach(tbx::ServiceProvider& service_provider) override;
    };
}
