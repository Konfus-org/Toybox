#pragma once
#include "player_input.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/vectors.h"

namespace three_d_example
{
    /// @brief
    /// Purpose: Applies player input to the character transform and child camera pitch.
    /// @details
    /// Assumption: The camera entity is authored as a child of the character entity.
    class PlayerCameraSystem final
    {
      public:
        PlayerCameraSystem(
            tbx::Entity character_entity,
            tbx::Entity camera_entity,
            float initial_yaw,
            float initial_pitch,
            float move_speed,
            float look_sensitivity);

      public:
        PlayerCameraSystem(const PlayerCameraSystem&) = delete;
        PlayerCameraSystem(PlayerCameraSystem&&) = delete;
        PlayerCameraSystem& operator=(const PlayerCameraSystem&) = delete;
        PlayerCameraSystem& operator=(PlayerCameraSystem&&) = delete;

      public:
        const tbx::Entity& get_camera() const;
        void update(const PlayerInput& input, const tbx::DeltaTime& dt);

      private:
        static tbx::Vec3 normalize_or_zero(const tbx::Vec3& value);

      private:
        tbx::Entity _camera_entity = {};
        tbx::Entity _character_entity = {};
        float _look_sensitivity = 0.0025F;
        float _move_speed = 6.0F;
        float _pitch = 0.0F;
        float _yaw = 0.0F;
    };
}
