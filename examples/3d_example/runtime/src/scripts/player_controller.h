#pragma once
#include "player_controller.generated.h"
#include "tbx/interfaces/input_manager.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/physics/physics.h"
#include "tbx/systems/scripting/script.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"
#include "tbx/types/vectors.h"
#include <memory>
#include <string>
#include <vector>

namespace three_d_example
{
    /// @brief
    /// Purpose: Tracks a projectile spawned by the player script.
    struct ProjectileInstance final
    {
        tbx::Entity entity = {};
        double remaining_lifetime_seconds = 0.0;
    };

    /// @brief
    /// Purpose: Script-owned player behavior for the authored 3D example character.
    [[script]];
    [[version(1U)]];
    class PlayerController final : public tbx::Script
    {
      public:
        PlayerController() = default;
        ~PlayerController() noexcept override = default;

      public:
        PlayerController(const PlayerController&) = delete;
        PlayerController& operator=(const PlayerController&) = delete;
        PlayerController(PlayerController&&) noexcept = delete;
        PlayerController& operator=(PlayerController&&) noexcept = delete;

      public:
        void on_destroy() override;
        void on_start() override;
        void on_update(const tbx::DeltaTime& dt) override;

      public:
        [[prop]]
        tbx::Uuid camera_entity = {};

        [[prop]]
        float initial_pitch = 0.0F;

        [[prop]]
        float initial_yaw = 0.0F;

        [[prop]]
        float look_sensitivity = 0.0025F;

        [[prop]]
        float move_speed = 6.0F;

        [[inject]]
        std::weak_ptr<tbx::IInputManager> input = {};

        [[inject]]
        std::weak_ptr<tbx::Physics> physics = {};

      private:
        tbx::InputAction create_look_action();
        tbx::InputAction create_move_action();
        tbx::InputAction create_raycast_action();
        tbx::InputAction create_shoot_action();
        tbx::InputAction create_vertical_move_action();
        tbx::MaterialInstance create_projectile_material() const;
        void setup_input();
        void reset_input_state();

        tbx::Vec3 get_shot_direction(const tbx::Transform& camera_transform) const;
        void cast_raycast() const;
        void spawn_projectile();
        void destroy_projectile(tbx::Entity& projectile) const;
        void remove_projectile_at(size projectile_index);

        void update_actions();
        void update_camera(const tbx::DeltaTime& dt);
        void update_projectiles(const tbx::DeltaTime& dt);

        // TODO: Make this a tbx vector util
        static tbx::Vec3 normalize_or_zero(const tbx::Vec3& value);

      private:
        std::string _scheme_name = "ThreeDExample.Player";
        std::weak_ptr<tbx::World> _world = {};
        tbx::Entity _camera_entity = {};
        tbx::MaterialInstance _projectile_material = {};
        tbx::Handle _projectile_model = {};
        std::vector<ProjectileInstance> _active_projectiles = {};
        tbx::Vec2 _look_delta = tbx::Vec2(0.0F, 0.0F);
        tbx::Vec2 _move_axis = tbx::Vec2(0.0F, 0.0F);
        tbx::Vec2 _vertical_axis = tbx::Vec2(0.0F, 0.0F);
        float _pitch = 0.0F;
        float _yaw = 0.0F;
        bool _raycast_requested = false;
        bool _shoot_requested = false;
        float _projectile_spawn_distance = 1.35F;
        float _projectile_speed = 26.0F;
        double _projectile_lifetime_seconds = 8.0;
        size _max_active_projectiles = 192U;
        size _spawned_projectile_count = 0U;
    };
}
