#pragma once
#include "tbx/ecs/entity.h"
#include "tbx/plugin_api/plugin.h"

namespace tbx::examples
{
    /// <summary>
    /// Purpose: Demonstrates runtime rigid-body simulation and transform sync behaviors.
    /// </summary>
    /// <remarks>
    /// Ownership: Holds non-owning references to host ECS services.
    /// Thread Safety: Must run on the main thread.
    /// </remarks>
    class PhysicsExampleRuntimePlugin final : public Plugin
    {
      public:
        void on_attach(IPluginHost& host) override;
    };
}
