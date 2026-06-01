#pragma once
#include "jolt_physics_backend.h"
#include "tbx/interfaces/physics_backend.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <memory>

namespace jolt_physics
{
    [[tbx::plugin(
        name = "JoltPhysicsPlugin",
        version = "1.0.0",
        category = tbx::PluginCategory::PHYSICS)]];
    class TBX_PLUGIN_API JoltPhysicsPlugin final : public tbx::Plugin
    {
      public:
        void on_detach() override;

      public:
        [[tbx::register(tbx::IPhysicsBackend)]]
        std::shared_ptr<JoltPhysicsBackend> physics_backend = {};
    };
}
