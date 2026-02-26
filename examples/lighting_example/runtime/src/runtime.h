#pragma once
#include "tbx/ecs/entity.h"
#include "tbx/examples/free_look_camera_controller.h"
#include "tbx/examples/room.h"
#include "tbx/plugin_api/plugin.h"
#include <vector>

namespace tbx::examples
{
    /// <summary>
    /// Purpose: Showcases directional, point, spot, and area lights with animated motion and color.
    /// </summary>
    /// <remarks>
    /// Ownership: Holds non-owning references and entity handles owned by the host ECS/input
    /// systems. Thread Safety: Must be used on the main thread where ECS and input are updated.
    /// </remarks>
    class LightingExampleRuntimePlugin final : public Plugin
    {
      public:
        /// <summary>
        /// Purpose: Initializes the scene, creates all light types, and enables camera controls.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of host services or entity storage.
        /// Thread Safety: Must be called on the main thread.
        /// </remarks>
        void on_attach(IPluginHost& host) override;

        /// <summary>
        /// Purpose: Cleans up input scheme state and clears non-owning runtime references.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not own host services; only releases local non-owning references.
        /// Thread Safety: Must be called on the main thread.
        /// </remarks>
        void on_detach() override;

        /// <summary>
        /// Purpose: Advances camera, light bobbing, directional rotation, and animated light
        /// colors.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of the provided DeltaTime instance.
        /// Thread Safety: Must be called on the main thread.
        /// </remarks>
        void on_update(const DeltaTime& dt) override;

      private:
        void rebuild_stress_lights(uint32 light_count);
        void clear_stress_lights();

        FreeLookCameraController _camera_controller = {};
        Room _room = {};

        Entity _directional_light = {};
        Entity _point_light = {};
        Entity _spot_light = {};
        Entity _area_light = {};

        Entity _point_light_marker = {};
        Entity _spot_light_marker = {};
        Entity _area_light_marker = {};

        Vec3 _point_base_position = Vec3(0.0F, 0.0F, 0.0F);
        Vec3 _spot_base_position = Vec3(0.0F, 0.0F, 0.0F);
        Vec3 _area_base_position = Vec3(0.0F, 0.0F, 0.0F);

        bool _directional_enabled = true;
        bool _point_enabled = false;
        bool _spot_enabled = false;
        bool _area_enabled = false;

        double _elapsed_seconds = 0.0;
        bool _stress_mode_enabled = false;
        uint32 _stress_light_count = 512U;
        std::vector<Entity> _stress_point_lights = {};
    };
}
