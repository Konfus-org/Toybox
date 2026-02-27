#pragma once
#include "tbx/ecs/entity.h"
#include "tbx/examples/player_controller.h"
#include "tbx/plugin_api/plugin.h"

namespace input_example
{
    /// <summary>
    /// Purpose: Demonstrates runtime input mapping by controlling a capsule player entity.
    /// </summary>
    /// <remarks>
    /// Ownership: Holds non-owning references to host services.
    /// Thread Safety: Must run on the main thread.
    /// </remarks>
    class InputExampleRuntimePlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::IPluginHost& host) override;
        void on_detach() override;
        void on_update(const tbx::DeltaTime& dt) override;

      private:
        tbx::Entity _sun = {};
        examples_common::PlayerController _player_controller = {};
    };
}
