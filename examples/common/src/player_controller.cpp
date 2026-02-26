#include "tbx/examples/player_controller.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/renderer.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"
#include <cmath>

namespace tbx::examples
{
    void PlayerController::initialize(
        EntityRegistry& entity_registry,
        InputManager& input_manager,
        const std::string& scheme_name,
        const PlayerControllerSettings& settings)
    {
        if (_is_initialized && !get_input_scheme_name().empty())
            input_manager.remove_scheme(get_input_scheme_name());

        bind_input_context(input_manager, scheme_name);
        _yaw = settings.initial_yaw;
        _pitch = settings.initial_pitch;
        _move_speed = settings.move_speed;
        _look_sensitivity = settings.look_sensitivity;
        _move_axis = Vec2(0.0F, 0.0F);
        _look_delta = Vec2(0.0F, 0.0F);

        _player = Entity("Player", entity_registry);
        _player.add_component<Transform>(settings.player_spawn_position);

        auto player_visual = Entity("PlayerVisual", _player.get_id(), entity_registry);
        player_visual.add_component<Renderer>(MaterialInstance {
            .parameters = {{"color", Color::RED}},
        });
        player_visual.add_component<DynamicMesh>(capsule);
        player_visual.add_component<Transform>(
            settings.visual_local_position,
            Quat(1.0F, 0.0F, 0.0F, 0.0F),
            settings.visual_local_scale);

        _camera = Entity("Camera", _player.get_id(), entity_registry);
        _camera.add_component<Camera>();
        _camera.add_component<Transform>(settings.camera_local_position);

        auto input_scheme = InputScheme(
            get_input_scheme_name(),
            {
                create_move_action(),
                create_look_action(),
            });

        input_manager.add_scheme(input_scheme);
        input_manager.activate_scheme(get_input_scheme_name());
        input_manager.set_mouse_lock_mode(MouseLockMode::RELATIVE);
        _is_initialized = true;
    }

    InputScheme& PlayerController::get_input_scheme()
    {
        return get_registered_input_scheme();
    }

    void PlayerController::shutdown(InputManager& input_manager)
    {
        if (!get_input_scheme_name().empty())
            input_manager.remove_scheme(get_input_scheme_name());
        input_manager.set_mouse_lock_mode(MouseLockMode::UNLOCKED);

        clear_input_context();
        _move_axis = Vec2(0.0F, 0.0F);
        _look_delta = Vec2(0.0F, 0.0F);
        _player = {};
        _camera = {};
        _is_initialized = false;
    }

    void PlayerController::update(const DeltaTime& dt)
    {
        if (!_player.get_id().is_valid() || !_camera.get_id().is_valid())
            return;

        auto& camera_transform = _camera.get_component<Transform>();
        auto& player_transform = _player.get_component<Transform>();

        _yaw -= _look_delta.x * _look_sensitivity;
        _pitch -= _look_delta.y * _look_sensitivity;

        const float max_pitch = to_radians(89.0F);
        if (_pitch > max_pitch)
            _pitch = max_pitch;
        if (_pitch < -max_pitch)
            _pitch = -max_pitch;

        auto yaw_rotation = normalize(Quat(Vec3(0.0F, _yaw, 0.0F)));
        auto pitch_rotation = normalize(Quat(Vec3(_pitch, 0.0F, 0.0F)));

        auto forward = yaw_rotation * Vec3(0.0F, 0.0F, -1.0F);
        auto right = yaw_rotation * Vec3(1.0F, 0.0F, 0.0F);
        forward.y = 0.0F;
        right.y = 0.0F;
        forward = normalize_or_zero(forward);
        right = normalize_or_zero(right);

        auto move = normalize_or_zero((forward * _move_axis.y) + (right * _move_axis.x));
        if (move.x != 0.0F || move.y != 0.0F || move.z != 0.0F)
            player_transform.position += move * _move_speed * static_cast<float>(dt.seconds);

        camera_transform.rotation = pitch_rotation;
        player_transform.rotation = yaw_rotation;
    }

    const Entity& PlayerController::get_player() const
    {
        return _player;
    }

    const Entity& PlayerController::get_camera() const
    {
        return _camera;
    }

    InputAction PlayerController::create_move_action()
    {
        return InputAction(
            "Move",
            InputActionValueType::VECTOR2,
            InputActionConstruction {
                .bindings =
                    {
                        InputBinding {
                            .control =
                                KeyboardVector2CompositeInputControl {
                                    .up = InputKey::W,
                                    .down = InputKey::S,
                                    .left = InputKey::A,
                                    .right = InputKey::D,
                                },
                            .scale = 1.0F,
                        },
                    },
                .on_performed_callbacks =
                    {
                        [this](const InputAction& action)
                        {
                            auto move_axis = Vec2(0.0F, 0.0F);
                            if (action.try_get_value_as<Vec2>(move_axis))
                                _move_axis = move_axis;
                        },
                    },
                .on_cancelled_callbacks =
                    {
                        [this](const InputAction&)
                        {
                            _move_axis = Vec2(0.0F, 0.0F);
                        },
                    },
            });
    }

    InputAction PlayerController::create_look_action()
    {
        return InputAction(
            "Look",
            InputActionValueType::VECTOR2,
            InputActionConstruction {
                .bindings =
                    {
                        InputBinding {
                            .control =
                                MouseVectorInputControl {
                                    .control = InputMouseVectorControl::DELTA,
                                },
                            .scale = 1.0F,
                        },
                    },
                .on_performed_callbacks =
                    {
                        [this](const InputAction& action)
                        {
                            auto look_delta = Vec2(0.0F, 0.0F);
                            if (action.try_get_value_as<Vec2>(look_delta))
                                _look_delta = look_delta;
                        },
                    },
                .on_cancelled_callbacks =
                    {
                        [this](const InputAction&)
                        {
                            _look_delta = Vec2(0.0F, 0.0F);
                        },
                    },
            });
    }

    Vec3 PlayerController::normalize_or_zero(const Vec3& value)
    {
        const float length_squared = value.x * value.x + value.y * value.y + value.z * value.z;
        if (length_squared <= 0.0F)
            return Vec3(0.0F, 0.0F, 0.0F);

        return value * (1.0F / std::sqrt(length_squared));
    }
}
