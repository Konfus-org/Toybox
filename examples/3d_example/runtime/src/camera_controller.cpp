#include "camera_controller.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/camera.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"
#include "tbx/physics/raycast.h"
#include <cmath>
#include <vector>

namespace three_d_example
{
    CameraController::CameraController(
        tbx::EntityRegistry& entity_registry,
        tbx::InputManager& input_manager,
        ProjectileSystem& projectile_system,
        const CameraControllerSettings& settings)
    {
        _entity_registry = &entity_registry;
        _input_manager = &input_manager;
        _projectile_system = &projectile_system;
        _scheme_name = "ThreeDExample.CameraController";
        _yaw = settings.initial_yaw;
        _pitch = settings.initial_pitch;
        _move_speed = settings.move_speed;
        _look_sensitivity = settings.look_sensitivity;

        create_entities(entity_registry, settings);
        register_input_scheme();
    }

    CameraController::~CameraController()
    {
        _entity_registry = nullptr;
        if (_input_manager != nullptr && !_scheme_name.empty())
            _input_manager->remove_scheme(_scheme_name);
        if (_input_manager != nullptr)
            _input_manager->set_mouse_lock_mode(tbx::MouseLockMode::UNLOCKED);

        _input_manager = nullptr;
        _projectile_system = nullptr;
        _scheme_name.clear();
        _camera_entity.destroy();
        _camera_entity = {};
        _character_entity.destroy();
        _character_entity = {};
        _move_axis = tbx::Vec2(0.0F, 0.0F);
        _vertical_axis = tbx::Vec2(0.0F, 0.0F);
        _look_delta = tbx::Vec2(0.0F, 0.0F);
    }

    void CameraController::update(const tbx::DeltaTime& dt)
    {
        if (!_character_entity.get_id().is_valid() || !_camera_entity.get_id().is_valid())
            return;

        _yaw -= _look_delta.x * _look_sensitivity;
        _pitch -= _look_delta.y * _look_sensitivity;

        const auto max_pitch = tbx::to_radians(89.0F);
        if (_pitch > max_pitch)
            _pitch = max_pitch;
        if (_pitch < -max_pitch)
            _pitch = -max_pitch;

        auto yaw_rotation = tbx::normalize(tbx::Quat(tbx::Vec3(0.0F, _yaw, 0.0F)));
        auto pitch_rotation = tbx::normalize(tbx::Quat(tbx::Vec3(_pitch, 0.0F, 0.0F)));

        auto character_transform = _character_entity.get_component<tbx::Transform>();
        auto forward = normalize_or_zero(yaw_rotation * tbx::Vec3(0.0F, 0.0F, -1.0F));
        auto right = normalize_or_zero(yaw_rotation * tbx::Vec3(1.0F, 0.0F, 0.0F));
        forward.y = 0.0F;
        right.y = 0.0F;
        forward = normalize_or_zero(forward);
        right = normalize_or_zero(right);

        auto move_direction = normalize_or_zero((forward * _move_axis.y) + (right * _move_axis.x));
        if (move_direction.x != 0.0F || move_direction.y != 0.0F || move_direction.z != 0.0F)
            character_transform.position +=
                move_direction * _move_speed * static_cast<float>(dt.seconds);

        character_transform.position +=
            tbx::Vec3(0.0F, _vertical_axis.y, 0.0F) * _move_speed * static_cast<float>(dt.seconds);

        character_transform.rotation = yaw_rotation;
        _character_entity.get_component<tbx::Transform>() = character_transform;
        _camera_entity.get_component<tbx::Transform>().rotation = pitch_rotation;
    }

    const tbx::Entity& CameraController::get_camera() const
    {
        return _camera_entity;
    }

    void CameraController::create_entities(
        tbx::EntityRegistry& entity_registry,
        const CameraControllerSettings& settings)
    {
        _character_entity = tbx::Entity("Character", entity_registry);
        _character_entity.add_component<tbx::Transform>(
            settings.initial_position,
            tbx::Quat(tbx::Vec3(0.0F, settings.initial_yaw, 0.0F)),
            tbx::Vec3(1.0F, 1.0F, 1.0F));

        _camera_entity = tbx::Entity("Camera", _character_entity.get_id(), entity_registry);
        _camera_entity.add_component<tbx::Camera>();
        _camera_entity.add_component<tbx::Transform>(
            tbx::Vec3(0.0F, 0.0F, 0.0F),
            tbx::Quat(tbx::Vec3(settings.initial_pitch, 0.0F, 0.0F)),
            tbx::Vec3(1.0F, 1.0F, 1.0F));
    }

    void CameraController::register_input_scheme()
    {
        if (_input_manager->get_scheme(_scheme_name) != nullptr)
            _input_manager->remove_scheme(_scheme_name);

        auto actions = std::vector<tbx::InputAction> {
            create_move_action(),
            create_look_action(),
            create_vertical_move_action(),
            create_raycast_action(),
            create_shoot_action(),
        };

        _input_manager->add_scheme(tbx::InputScheme(_scheme_name, actions));
        _input_manager->activate_scheme(_scheme_name);
        _input_manager->set_mouse_lock_mode(tbx::MouseLockMode::RELATIVE);
    }

    tbx::InputAction CameraController::create_move_action()
    {
        return tbx::InputAction(
            "Move",
            tbx::InputActionValueType::VECTOR2,
            tbx::InputActionConstruction {
                .bindings =
                    {
                        tbx::InputBinding {
                            .control =
                                tbx::KeyboardVector2CompositeInputControl {
                                    .up = tbx::InputKey::W,
                                    .down = tbx::InputKey::S,
                                    .left = tbx::InputKey::A,
                                    .right = tbx::InputKey::D,
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

    tbx::InputAction CameraController::create_look_action()
    {
        return tbx::InputAction(
            "Look",
            tbx::InputActionValueType::VECTOR2,
            tbx::InputActionConstruction {
                .bindings =
                    {
                        tbx::InputBinding {
                            .control =
                                tbx::MouseVectorInputControl {
                                    .control = tbx::InputMouseVectorControl::DELTA,
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

    tbx::InputAction CameraController::create_vertical_move_action()
    {
        return tbx::InputAction(
            "VerticalMove",
            tbx::InputActionValueType::VECTOR2,
            tbx::InputActionConstruction {
                .bindings =
                    {
                        tbx::InputBinding {
                            .control =
                                tbx::KeyboardVector2CompositeInputControl {
                                    .up = tbx::InputKey::Q,
                                    .down = tbx::InputKey::E,
                                },
                            .scale = 1.0F,
                        },
                    },
                .on_performed_callbacks =
                    {
                        [this](const tbx::InputAction& action)
                        {
                            auto vertical_axis = tbx::Vec2(0.0F, 0.0F);
                            if (action.try_get_value_as<tbx::Vec2>(vertical_axis))
                                _vertical_axis = vertical_axis;
                        },
                    },
                .on_cancelled_callbacks =
                    {
                        [this](const tbx::InputAction&)
                        {
                            _vertical_axis = tbx::Vec2(0.0F, 0.0F);
                        },
                    },
            });
    }

    tbx::InputAction CameraController::create_raycast_action()
    {
        return tbx::InputAction(
            "Raycast",
            tbx::InputActionValueType::BUTTON,
            tbx::InputActionConstruction {
                .bindings =
                    {
                        tbx::InputBinding {
                            .control =
                                tbx::MouseButtonInputControl {
                                    .button = tbx::InputMouseButton::LEFT,
                                },
                            .scale = 1.0F,
                        },
                    },
                .on_start_callbacks =
                    {
                        [this](const tbx::InputAction&)
                        {
                            cast_raycast();
                        },
                    },
            });
    }

    tbx::InputAction CameraController::create_shoot_action()
    {
        return tbx::InputAction(
            "Shoot",
            tbx::InputActionValueType::BUTTON,
            tbx::InputActionConstruction {
                .bindings =
                    {
                        tbx::InputBinding {
                            .control =
                                tbx::MouseButtonInputControl {
                                    .button = tbx::InputMouseButton::RIGHT,
                                },
                            .scale = 1.0F,
                        },
                    },
                .on_start_callbacks =
                    {
                        [this](const tbx::InputAction&)
                        {
                            if (_projectile_system != nullptr)
                                _projectile_system->request_spawn();
                        },
                    },
            });
    }

    void CameraController::cast_raycast() const
    {
        if (_entity_registry == nullptr || !_camera_entity.get_id().is_valid())
            return;

        const auto camera_world_transform = tbx::get_world_space_transform(_camera_entity);
        const auto direction =
            tbx::normalize(camera_world_transform.rotation * tbx::Vec3(0.0F, 0.0F, -1.0F));

        auto raycast = tbx::Raycast {
            .origin = camera_world_transform.position,
            .direction = direction,
            .max_distance = 60.0F,
            .ignore_entity = true,
            .ignored_entity_id = _camera_entity.get_id(),
        };

        const auto raycast_result = raycast.cast();
        if (!raycast_result)
        {
            TBX_TRACE_INFO("Raycast missed.");
            return;
        }

        TBX_TRACE_INFO(
            "Raycast hit entity {} at ({:.2f}, {:.2f}, {:.2f}).",
            _entity_registry->get(raycast_result.hit_entity_id).get_name(),
            raycast_result.hit_position.x,
            raycast_result.hit_position.y,
            raycast_result.hit_position.z);
    }

    tbx::Vec3 CameraController::normalize_or_zero(const tbx::Vec3& value)
    {
        const auto length_squared = value.x * value.x + value.y * value.y + value.z * value.z;
        if (length_squared <= 0.0F)
            return tbx::Vec3(0.0F, 0.0F, 0.0F);

        return value * (1.0F / std::sqrt(length_squared));
    }
}
