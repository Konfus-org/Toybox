#pragma once
#include "projectile_system.h"
#include "tbx/interfaces/input_manager.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/physics/physics.h"
#include "tbx/systems/time/delta_time.h"
#include <memory>
#include <string>

namespace three_d_example
{
    struct CameraControllerSettings final
    {
        tbx::Vec3 initial_position = tbx::Vec3(0.0F, 2.01F, 11.0F);
        float initial_yaw = 0.0F;
        float initial_pitch = 0.0F;
        float move_speed = 6.0F;
        float look_sensitivity = 0.0025F;
    };

    class CameraController final
    {
      public:
        CameraController(
            std::weak_ptr<tbx::EntityRegistry> entity_registry,
            std::weak_ptr<tbx::IInputManager> input_manager,
            std::weak_ptr<tbx::Physics> physics,
            std::weak_ptr<ProjectileSystem> projectile_system,
            const CameraControllerSettings& settings);
        ~CameraController();

        CameraController(const CameraController&) = delete;
        CameraController(CameraController&&) = delete;
        CameraController& operator=(const CameraController&) = delete;
        CameraController& operator=(CameraController&&) = delete;

        void update(const tbx::DeltaTime& dt);
        const tbx::Entity& get_camera() const;

      private:
        void create_entities(
            tbx::EntityRegistry& entity_registry,
            const CameraControllerSettings& settings);
        void register_input_scheme();
        tbx::InputAction create_move_action();
        tbx::InputAction create_look_action();
        tbx::InputAction create_vertical_move_action();
        tbx::InputAction create_flashlight_toggle_action();
        tbx::InputAction create_raycast_action();
        tbx::InputAction create_shoot_action();
        void cast_raycast() const;
        void set_flashlight_enabled(bool is_enabled);
        static tbx::Vec3 normalize_or_zero(const tbx::Vec3& value);

      private:
        std::weak_ptr<tbx::EntityRegistry> _entity_registry = {};
        std::weak_ptr<tbx::IInputManager> _input_manager = {};
        std::weak_ptr<tbx::Physics> _physics = {};
        std::weak_ptr<ProjectileSystem> _projectile_system = {};
        std::string _scheme_name = {};
        tbx::Entity _character_entity = {};
        tbx::Entity _camera_entity = {};
        tbx::Entity _reticle_entity = {};
        tbx::Vec2 _move_axis = tbx::Vec2(0.0F, 0.0F);
        tbx::Vec2 _vertical_axis = tbx::Vec2(0.0F, 0.0F);
        tbx::Vec2 _look_delta = tbx::Vec2(0.0F, 0.0F);
        float _yaw = 0.0F;
        float _pitch = 0.0F;
        float _move_speed = 6.0F;
        float _look_sensitivity = 0.0025F;
        bool _is_flashlight_enabled = false;
        float _flashlight_intensity = 180.0F;
    };
}
