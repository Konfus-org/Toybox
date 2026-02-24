#pragma once
#include "tbx/ecs/entity.h"
#include "tbx/input/input_manager.h"
#include "tbx/plugin_api/plugin.h"
#include <string>

namespace tbx::examples
{
    /// <summary>
    /// Purpose: Demonstrates runtime input mapping by controlling a capsule player entity.
    /// </summary>
    /// <remarks>
    /// Ownership: Holds non-owning pointers to host services.
    /// Thread Safety: Must run on the main thread.
    /// </remarks>
    class InputExampleRuntimePlugin final : public Plugin
    {
      public:
        void on_attach(IPluginHost& host) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;

      private:
        EntityRegistry* _ent_registry = nullptr;
        InputManager* _input_manager = nullptr;

        Entity _player = {};
        Entity _camera = {};
        Entity _sun = {};

        std::string _camera_scheme_name = "InputExample.Camera";

        float _camera_yaw = 0.0F;
        float _camera_pitch = 0.0F;
        float _player_move_speed = 6.0F;
        float _camera_look_sensitivity = 0.0025F;

        Vec2 _move_axis = Vec2(0.0F, 0.0F);
        Vec2 _look_delta = Vec2(0.0F, 0.0F);
    };
}
