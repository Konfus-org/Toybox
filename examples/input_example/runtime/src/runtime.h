#pragma once
#include "tbx/ecs/entity.h"
#include "tbx/examples/player_controller.h"
#include "tbx/plugin_api/plugin.h"

namespace tbx::examples
{
    /// <summary>
    /// Purpose: Demonstrates runtime input mapping by controlling a capsule player entity.
    /// </summary>
    /// <remarks>
    /// Ownership: Holds non-owning references to host services.
    /// Thread Safety: Must run on the main thread.
    /// </remarks>
    class InputExampleRuntimePlugin final : public Plugin
    {
      public:
        void on_attach(IPluginHost& host) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;

      private:
        Entity _sun = {};
        PlayerController _player_controller = {};
    };
}
