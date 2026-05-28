#include "player.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/raycast.h"
#include <utility>

namespace three_d_example
{
    Player::Player(
        std::weak_ptr<tbx::World> world,
        tbx::Entity character_entity,
        tbx::Entity camera_entity,
        std::weak_ptr<tbx::IInputManager> input_manager,
        std::weak_ptr<tbx::Physics> physics,
        const PlayerSettings& settings)
        : _camera_system(
              std::move(character_entity),
              camera_entity,
              settings.initial_yaw,
              settings.initial_pitch,
              settings.move_speed,
              settings.look_sensitivity)
        , _flashlight_system(
              world,
              std::move(camera_entity),
              settings.flashlight_follow_speed,
              settings.flashlight_intensity)
        , _input(input_manager)
        , _physics(physics)
        , _projectile_system(
              world,
              [this]()
              {
                  return _camera_system.get_camera();
              })
        , _world(world)
    {
    }

    const tbx::Entity& Player::get_camera() const
    {
        return _camera_system.get_camera();
    }

    void Player::update(const tbx::DeltaTime& dt)
    {
        _camera_system.update(_input, dt);
        update_actions();
        _flashlight_system.update(dt);
        _projectile_system.update(dt);
    }

    void Player::cast_raycast() const
    {
        auto world = _world.lock();
        auto physics = _physics.lock();
        const auto& camera = _camera_system.get_camera();
        if (!world || !physics || !camera.get_id().is_valid())
            return;

        const auto camera_world_transform = tbx::get_world_space_transform(camera);
        const auto direction =
            tbx::normalize(camera_world_transform.rotation * tbx::Vec3(0.0F, 0.0F, -1.0F));

        auto raycast = tbx::RaycastQuery {
            .origin = camera_world_transform.position,
            .direction = direction,
            .max_distance = 60.0F,
            .ignore_entity = true,
            .ignored_entity_id = camera.get_id(),
        };

        const auto raycast_result = physics->raycast(raycast);
        if (!raycast_result)
        {
            TBX_TRACE_INFO("Raycast missed.");
            return;
        }

        TBX_TRACE_INFO(
            "Raycast hit entity {} at ({:.2f}, {:.2f}, {:.2f}).",
            world->get(raycast_result.hit_entity_id).get_name(),
            raycast_result.hit_position.x,
            raycast_result.hit_position.y,
            raycast_result.hit_position.z);
    }

    void Player::update_actions()
    {
        if (_input.consume_flashlight_toggle())
            _flashlight_system.set_enabled(!_flashlight_system.get_is_enabled());

        if (_input.consume_raycast_request())
            cast_raycast();

        if (_input.consume_shoot_request())
            _projectile_system.request_spawn();
    }
}
