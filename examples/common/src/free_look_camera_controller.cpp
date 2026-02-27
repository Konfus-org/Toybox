#include "tbx/examples/free_look_camera_controller.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"
#include <cmath>
#include <string>
#include <vector>

namespace examples_common
{
    void FreeLookCameraController::initialize(
        tbx::Entity camera,
        const FreeLookCameraControllerSettings& settings)
    {
        clear_input_context();
        _camera_entity = camera;
        _yaw = settings.initial_yaw;
        _pitch = settings.initial_pitch;
        _move_speed = settings.move_speed;
        _look_sensitivity = settings.look_sensitivity;
        _move_axis = tbx::Vec2(0.0F, 0.0F);
        _up_down_axis = tbx::Vec2(0.0F, 0.0F);
        _look_delta = tbx::Vec2(0.0F, 0.0F);
    }

    void FreeLookCameraController::initialize(
        tbx::Entity camera,
        InputManager& input_manager,
        const FreeLookCameraControllerSettings& settings)
    {
        initialize(camera, settings);

        const auto scheme_name = std::string("FreeLookCamera");
        if (input_manager.get_scheme(scheme_name) != nullptr)
            input_manager.remove_scheme(scheme_name);

        bind_input_context(input_manager, scheme_name);

        auto actions = std::vector<tbx::InputAction> {
            create_move_action(),
            create_look_action(),
        };
        actions.push_back(create_up_down_action());
        input_manager.add_scheme(tbx::InputScheme(scheme_name, actions));
        input_manager.activate_scheme(scheme_name);
        input_manager.set_mouse_lock_mode(MouseLockMode::RELATIVE);
    }

    void FreeLookCameraController::shutdown(InputManager& input_manager)
    {
        if (!get_input_scheme_name().empty())
            input_manager.remove_scheme(get_input_scheme_name());
        input_manager.set_mouse_lock_mode(MouseLockMode::UNLOCKED);

        clear_input_context();
        _move_axis = tbx::Vec2(0.0F, 0.0F);
        _up_down_axis = tbx::Vec2(0.0F, 0.0F);
        _look_delta = tbx::Vec2(0.0F, 0.0F);
        _camera_entity = {};
    }

    tbx::InputScheme& FreeLookCameraController::get_input_scheme() const
    {
        return get_registered_input_scheme();
    }

    void FreeLookCameraController::update(const tbx::DeltaTime& dt)
    {
        if (!_camera_entity.get_id().is_valid())
            return;

        _yaw -= _look_delta.x * _look_sensitivity;
        _pitch -= _look_delta.y * _look_sensitivity;

        const float max_pitch = tbx::to_radians(89.0F);
        if (_pitch > max_pitch)
            _pitch = max_pitch;
        if (_pitch < -max_pitch)
            _pitch = -max_pitch;

        auto yaw_rotation = normalize(tbx::Quat(tbx::Vec3(0.0F, _yaw, 0.0F)));
        auto pitch_rotation = normalize(tbx::Quat(tbx::Vec3(_pitch, 0.0F, 0.0F)));

        auto& camera_transform = _camera_entity.get_component<tbx::Transform>();
        auto forward = normalize_or_zero(yaw_rotation * tbx::Vec3(0.0F, 0.0F, -1.0F));
        auto right = normalize_or_zero(yaw_rotation * tbx::Vec3(1.0F, 0.0F, 0.0F));
        forward.y = 0.0F;
        right.y = 0.0F;
        forward = normalize_or_zero(forward);
        right = normalize_or_zero(right);

        auto move_direction = normalize_or_zero((forward * _move_axis.y) + (right * _move_axis.x));
        if (move_direction.x != 0.0F || move_direction.y != 0.0F || move_direction.z != 0.0F)
            camera_transform.position +=
                move_direction * _move_speed * static_cast<float>(dt.seconds);

        camera_transform.position +=
            tbx::Vec3(0.0F, _up_down_axis.y, 0.0F) * _move_speed * static_cast<float>(dt.seconds);

        camera_transform.rotation = normalize(yaw_rotation * pitch_rotation);
    }

    const tbx::Entity& FreeLookCameraController::get_camera() const
    {
        return _camera_entity;
    }

    tbx::InputAction FreeLookCameraController::create_move_action()
    {
        return tbx::InputAction(
            "Move",
            InputActionValueType::VECTOR2,
            InputActionConstruction {
                .bindings =
                    {
                        tbx::InputBinding {
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
                        [this](const tbx::InputAction& action)
                        {
                            auto move_axis = tbx::Vec2(0.0F, 0.0F);
                            if (action.try_get_value_as<tbx::Vec2>(move_axis))
                                _move_axis = move_axis;
                        },
                    },
                .on_cancelled_callbacks =
                    {
                        [this](const tbx::InputAction&)
                        {
                            _move_axis = tbx::Vec2(0.0F, 0.0F);
                        },
                    },
            });
    }

    tbx::InputAction FreeLookCameraController::create_look_action()
    {
        return tbx::InputAction(
            "Look",
            InputActionValueType::VECTOR2,
            InputActionConstruction {
                .bindings =
                    {
                        tbx::InputBinding {
                            .control =
                                MouseVectorInputControl {
                                    .control = InputMouseVectorControl::DELTA,
                                },
                            .scale = 1.0F,
                        },
                    },
                .on_performed_callbacks =
                    {
                        [this](const tbx::InputAction& action)
                        {
                            auto look_delta = tbx::Vec2(0.0F, 0.0F);
                            if (action.try_get_value_as<tbx::Vec2>(look_delta))
                                _look_delta = look_delta;
                        },
                    },
                .on_cancelled_callbacks =
                    {
                        [this](const tbx::InputAction&)
                        {
                            _look_delta = tbx::Vec2(0.0F, 0.0F);
                        },
                    },
            });
    }

    tbx::InputAction FreeLookCameraController::create_up_down_action()
    {
        return tbx::InputAction(
            "UpDown",
            InputActionValueType::VECTOR2,
            InputActionConstruction {
                .bindings =
                    {
                        tbx::InputBinding {
                            .control =
                                KeyboardVector2CompositeInputControl {
                                    .up = InputKey::Q,
                                    .down = InputKey::E,
                                },
                            .scale = 1.0F,
                        },
                    },
                .on_performed_callbacks =
                    {
                        [this](const tbx::InputAction& action)
                        {
                            auto up_down_axis = tbx::Vec2(0.0F, 0.0F);
                            if (action.try_get_value_as<tbx::Vec2>(up_down_axis))
                                _up_down_axis = up_down_axis;
                        },
                    },
                .on_cancelled_callbacks =
                    {
                        [this](const tbx::InputAction&)
                        {
                            _up_down_axis = tbx::Vec2(0.0F, 0.0F);
                        },
                    },
            });
    }

    tbx::Vec3 FreeLookCameraController::normalize_or_zero(const tbx::Vec3& value)
    {
        const float length_squared = value.x * value.x + value.y * value.y + value.z * value.z;
        if (length_squared <= 0.0F)
            return tbx::Vec3(0.0F, 0.0F, 0.0F);

        return value * (1.0F / std::sqrt(length_squared));
    }
}
