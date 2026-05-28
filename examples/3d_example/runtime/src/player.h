#pragma once
#include "flashlight_system.h"
#include "player_camera_system.h"
#include "player_input.h"
#include "player_settings.h"
#include "projectile_system.h"
#include "tbx/interfaces/input_manager.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/physics/physics.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/assets/world.h"

namespace three_d_example
{
    /// @brief
    /// Purpose: Composes the player input, camera, flashlight, raycast, and projectile systems.
    class Player final
    {
      public:
        Player(
            std::weak_ptr<tbx::World> world,
            tbx::Entity character_entity,
            tbx::Entity camera_entity,
            std::weak_ptr<tbx::IInputManager> input_manager,
            std::weak_ptr<tbx::Physics> physics,
            const PlayerSettings& settings);

      public:
        Player(const Player&) = delete;
        Player(Player&&) = delete;
        Player& operator=(const Player&) = delete;
        Player& operator=(Player&&) = delete;

      public:
        const tbx::Entity& get_camera() const;
        void update(const tbx::DeltaTime& dt);

      private:
        void cast_raycast() const;
        void update_actions();

      private:
        PlayerCameraSystem _camera_system;
        FlashlightSystem _flashlight_system;
        PlayerInput _input;
        std::weak_ptr<tbx::Physics> _physics = {};
        ProjectileSystem _projectile_system;
        std::weak_ptr<tbx::World> _world = {};
    };
}
