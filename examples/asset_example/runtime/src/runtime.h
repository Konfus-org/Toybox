#pragma once
#include "tbx/ecs/entities.h"
#include "tbx/input/input_manager.h"
#include "tbx/plugin_api/plugin.h"
#include <string>

namespace tbx::examples
{
    /// <summary>
    /// Runtime plugin that loads the asset scene and drives camera controls using input actions.
    /// </summary>
    /// <remarks>
    /// Purpose: Demonstrates asset loading plus a practical input-scheme-driven camera controller.
    /// Ownership: Holds non-owning pointers to ECS and input manager services provided by the host.
    /// Thread Safety: Expects to run on the main thread where the ECS and input systems are
    /// updated.
    /// </remarks>
    class AssetExampleRuntimePlugin final : public Plugin
    {
      public:
        /// <summary>
        /// Initializes the example entities, scene assets, and camera input scheme.
        /// </summary>
        /// <remarks>
        /// Purpose: Creates scene entities and registers/activates a camera input scheme.
        /// Ownership: Does not take ownership of host services.
        /// Thread Safety: Must be called on the main thread.
        /// </remarks>
        void on_attach(IPluginHost& host) override;

        /// <summary>
        /// Releases cached runtime references and camera input scheme state.
        /// </summary>
        /// <remarks>
        /// Purpose: Clears non-owning references and de-registers the example camera scheme.
        /// Ownership: Does not own host services or entity storage.
        /// Thread Safety: Must be called on the main thread.
        /// </remarks>
        void on_detach() override;

        /// <summary>
        /// Updates scene animation and input-driven camera movement/rotation.
        /// </summary>
        /// <remarks>
        /// Purpose: Rotates demo entities and applies WASD + mouse-look camera motion.
        /// Ownership: Does not own the provided DeltaTime instance.
        /// Thread Safety: Must be called on the main thread.
        /// </remarks>
        void on_update(const DeltaTime& dt) override;

      private:
        EntityRegistry* _ent_registry = nullptr;
        InputManager* _input_manager = nullptr;

        Entity _green_cube = {};
        Entity _sun = {};
        Entity _camera = {};

        std::string _camera_scheme_name = "AssetExample.Camera";

        float _camera_yaw = 0.0F;
        float _camera_pitch = 0.0F;
        float _camera_move_speed = 6.0F;
        float _camera_look_sensitivity = 0.0025F;

        Vec2 _move_axis = Vec2(0.0F, 0.0F);
        Vec2 _look_delta = Vec2(0.0F, 0.0F);
    };
}
