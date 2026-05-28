#pragma once
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/components/light.h"

namespace three_d_example
{
    /// @brief
    /// Purpose: Owns the runtime flashlight and follows the player camera with a slight lag.
    /// @details
    /// Assumption: If the authored camera has a SpotLight, its settings seed this runtime light
    /// and the component is removed from the camera.
    class FlashlightSystem final
    {
      public:
        FlashlightSystem(
            std::weak_ptr<tbx::World> world,
            tbx::Entity camera_entity,
            float follow_speed,
            float light_intensity);
        ~FlashlightSystem();

      public:
        FlashlightSystem(const FlashlightSystem&) = delete;
        FlashlightSystem(FlashlightSystem&&) = delete;
        FlashlightSystem& operator=(const FlashlightSystem&) = delete;
        FlashlightSystem& operator=(FlashlightSystem&&) = delete;

      public:
        bool get_is_enabled() const;
        void set_enabled(bool is_enabled);
        void update(const tbx::DeltaTime& dt);

      private:
        tbx::SpotLight create_spot_light(tbx::Entity& camera_entity) const;
        float get_frame_blend(const tbx::DeltaTime& dt) const;
        void sync_to_camera_now();

      private:
        tbx::Entity _camera_entity = {};
        tbx::Entity _flashlight_entity = {};
        float _follow_speed = 12.0F;
        bool _is_enabled = false;
        float _light_intensity = 180.0F;
        std::weak_ptr<tbx::World> _world = {};
    };
}
