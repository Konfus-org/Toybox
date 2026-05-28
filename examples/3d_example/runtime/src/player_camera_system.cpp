#include "player_camera_system.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/trig.h"

namespace three_d_example
{
    constexpr float ROOM_INNER_HALF_EXTENT = 18.0F;

    PlayerCameraSystem::PlayerCameraSystem(
        tbx::Entity character_entity,
        tbx::Entity camera_entity,
        float initial_yaw,
        float initial_pitch,
        float move_speed,
        float look_sensitivity)
        : _camera_entity(std::move(camera_entity))
        , _character_entity(std::move(character_entity))
        , _look_sensitivity(look_sensitivity)
        , _move_speed(move_speed)
        , _pitch(initial_pitch)
        , _yaw(initial_yaw)
    {
        if (_character_entity.get_id().is_valid()
            && _character_entity.has_component<tbx::Transform>())
        {
            auto& transform = _character_entity.get_component<tbx::Transform>();
            transform.rotation = tbx::Quat(tbx::Vec3(0.0F, _yaw, 0.0F));
        }

        if (_camera_entity.get_id().is_valid() && _camera_entity.has_component<tbx::Transform>())
        {
            auto& transform = _camera_entity.get_component<tbx::Transform>();
            transform.rotation = tbx::Quat(tbx::Vec3(_pitch, 0.0F, 0.0F));
        }
    }

    const tbx::Entity& PlayerCameraSystem::get_camera() const
    {
        return _camera_entity;
    }

    void PlayerCameraSystem::update(const PlayerInput& input, const tbx::DeltaTime& dt)
    {
        if (!_character_entity.get_id().is_valid() || !_camera_entity.get_id().is_valid())
            return;
        if (!_character_entity.has_component<tbx::Transform>()
            || !_camera_entity.has_component<tbx::Transform>())
        {
            return;
        }

        _yaw -= input.get_look_delta().x * _look_sensitivity;
        _pitch -= input.get_look_delta().y * _look_sensitivity;

        const auto max_pitch = tbx::to_radians(89.0F);
        if (_pitch > max_pitch)
            _pitch = max_pitch;
        if (_pitch < -max_pitch)
            _pitch = -max_pitch;

        const auto yaw_rotation = tbx::normalize(tbx::Quat(tbx::Vec3(0.0F, _yaw, 0.0F)));
        const auto pitch_rotation = tbx::normalize(tbx::Quat(tbx::Vec3(_pitch, 0.0F, 0.0F)));

        auto character_transform = _character_entity.get_component<tbx::Transform>();
        auto forward = normalize_or_zero(yaw_rotation * tbx::Vec3(0.0F, 0.0F, -1.0F));
        auto right = normalize_or_zero(yaw_rotation * tbx::Vec3(1.0F, 0.0F, 0.0F));
        forward.y = 0.0F;
        right.y = 0.0F;
        forward = normalize_or_zero(forward);
        right = normalize_or_zero(right);

        const auto move_direction = normalize_or_zero(
            (forward * input.get_move_axis().y) + (right * input.get_move_axis().x));
        if (move_direction.x != 0.0F || move_direction.y != 0.0F || move_direction.z != 0.0F)
        {
            character_transform.position +=
                move_direction * _move_speed * static_cast<float>(dt.seconds);
        }

        character_transform.position += tbx::Vec3(0.0F, input.get_vertical_axis().y, 0.0F)
                                        * _move_speed * static_cast<float>(dt.seconds);
        // The authored demo room walls sit at +/-20 with thickness. Keep the free-fly camera
        // inside their inner faces so movement respects the visible room bounds.
        character_transform.position.x = std::clamp(
            character_transform.position.x,
            -ROOM_INNER_HALF_EXTENT,
            ROOM_INNER_HALF_EXTENT);
        character_transform.position.z = std::clamp(
            character_transform.position.z,
            -ROOM_INNER_HALF_EXTENT,
            ROOM_INNER_HALF_EXTENT);
        character_transform.rotation = yaw_rotation;

        _character_entity.get_component<tbx::Transform>() = character_transform;
        _camera_entity.get_component<tbx::Transform>().rotation = pitch_rotation;
    }

    tbx::Vec3 PlayerCameraSystem::normalize_or_zero(const tbx::Vec3& value)
    {
        const auto length_squared = value.x * value.x + value.y * value.y + value.z * value.z;
        if (length_squared <= 0.0F)
            return tbx::Vec3(0.0F, 0.0F, 0.0F);

        return value * (1.0F / std::sqrt(length_squared));
    }
}
