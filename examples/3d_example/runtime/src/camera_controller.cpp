#include "camera_controller.h"
#include "tbx/systems/assets/builtin_assets.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/light.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/material.h"
#include "tbx/types/raycast.h"
#include "tbx/types/trig.h"
#include <cmath>
#include <vector>

namespace three_d_example
{
    CameraController::CameraController(
        std::weak_ptr<tbx::EntityRegistry> entity_registry,
        std::weak_ptr<tbx::IInputManager> input_manager,
        std::weak_ptr<tbx::Physics> physics,
        std::weak_ptr<ProjectileSystem> projectile_system,
        const CameraControllerSettings& settings)
    {
        _entity_registry = entity_registry;
        _input_manager = input_manager;
        _physics = physics;
        _projectile_system = projectile_system;
        _scheme_name = "ThreeDExample.CameraController";
        _yaw = settings.initial_yaw;
        _pitch = settings.initial_pitch;
        _move_speed = settings.move_speed;
        _look_sensitivity = settings.look_sensitivity;

        auto entity_registry_lock = _entity_registry.lock();
        if (!entity_registry_lock)
            return;

        create_entities(*entity_registry_lock, settings);
        register_input_scheme();
    }

    CameraController::~CameraController()
    {
        auto input_manager = _input_manager.lock();
        if (input_manager && !_scheme_name.empty())
            input_manager->remove_scheme(_scheme_name);
        if (input_manager)
            input_manager->set_mouse_lock_mode(tbx::MouseLockMode::UNLOCKED);

        _entity_registry.reset();
        _input_manager.reset();
        _physics.reset();
        _projectile_system.reset();
        _scheme_name.clear();
        _reticle_entity.destroy();
        _reticle_entity = {};
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
        auto flashlight = tbx::SpotLight(tbx::Color::WHITE, 0.0F, 48.0F, 16.0F, 28.0F);
        flashlight.cast_shadows = false;
        _camera_entity.add_component<tbx::SpotLight>(flashlight);
        _camera_entity.add_component<tbx::Transform>(
            tbx::Vec3(0.0F, 0.0F, 0.0F),
            tbx::Quat(tbx::Vec3(settings.initial_pitch, 0.0F, 0.0F)),
            tbx::Vec3(1.0F, 1.0F, 1.0F));

        _reticle_entity = tbx::Entity("Reticle", _camera_entity.get_id(), entity_registry);
        auto reticle_material = tbx::MaterialInstance(tbx::FlatMaterial::HANDLE);
        reticle_material.set_parameter(tbx::FlatMaterial::ALBEDO_COLOR, tbx::Color::WHITE);
        _reticle_entity.add_component<tbx::MaterialInstance>(reticle_material);
        _reticle_entity.add_component<tbx::DynamicMesh>(tbx::Mesh::QUAD);
        _reticle_entity.add_component<tbx::Transform>(
            tbx::Vec3(0.0F, 0.0F, -0.3F),
            tbx::Quat(tbx::Vec3(0.0F, 0.0F, 0.0F)),
            tbx::Vec3(0.01F, 0.01F, 0.01F));

        set_flashlight_enabled(false);
    }

    void CameraController::register_input_scheme()
    {
        auto input_manager = _input_manager.lock();
        if (!input_manager)
            return;

        if (input_manager->get_scheme(_scheme_name).has_value())
            input_manager->remove_scheme(_scheme_name);

        auto actions = std::vector<tbx::InputAction> {
            create_move_action(),
            create_look_action(),
            create_vertical_move_action(),
            create_flashlight_toggle_action(),
            create_raycast_action(),
            create_shoot_action(),
        };

        input_manager->add_scheme(tbx::InputScheme(_scheme_name, actions));
        input_manager->activate_scheme(_scheme_name);
        input_manager->set_mouse_lock_mode(tbx::MouseLockMode::RELATIVE);
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

    tbx::InputAction CameraController::create_flashlight_toggle_action()
    {
        return tbx::InputAction(
            "ToggleFlashlight",
            tbx::InputActionValueType::BUTTON,
            tbx::InputActionConstruction {
                .bindings =
                    {
                        tbx::InputBinding {
                            .control = tbx::KeyboardInputControl {.key = tbx::InputKey::F},
                            .scale = 1.0F,
                        },
                    },
                .on_start_callbacks =
                    {
                        [this](const tbx::InputAction&)
                        {
                            set_flashlight_enabled(!_is_flashlight_enabled);
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
                            auto projectile_system = _projectile_system.lock();
                            if (projectile_system)
                                projectile_system->request_spawn();
                        },
                    },
            });
    }

    void CameraController::cast_raycast() const
    {
        auto entity_registry = _entity_registry.lock();
        auto physics = _physics.lock();
        if (!entity_registry || !physics || !_camera_entity.get_id().is_valid())
            return;

        const auto camera_world_transform = tbx::get_world_space_transform(_camera_entity);
        const auto direction =
            tbx::normalize(camera_world_transform.rotation * tbx::Vec3(0.0F, 0.0F, -1.0F));

        auto raycast = tbx::RaycastQuery {
            .origin = camera_world_transform.position,
            .direction = direction,
            .max_distance = 60.0F,
            .ignore_entity = true,
            .ignored_entity_id = _camera_entity.get_id(),
        };

        const auto raycast_result = physics->raycast(raycast);
        if (!raycast_result)
        {
            TBX_TRACE_INFO("Raycast missed.");
            return;
        }

        TBX_TRACE_INFO(
            "Raycast hit entity {} at ({:.2f}, {:.2f}, {:.2f}).",
            entity_registry->get(raycast_result.hit_entity_id).get_name(),
            raycast_result.hit_position.x,
            raycast_result.hit_position.y,
            raycast_result.hit_position.z);
    }

    void CameraController::set_flashlight_enabled(bool is_enabled)
    {
        _is_flashlight_enabled = is_enabled;

        if (!_camera_entity.get_id().is_valid() || !_camera_entity.has_component<tbx::SpotLight>())
            return;

        auto& flashlight = _camera_entity.get_component<tbx::SpotLight>();
        flashlight.intensity = _is_flashlight_enabled ? _flashlight_intensity : 0.0F;
    }

    tbx::Vec3 CameraController::normalize_or_zero(const tbx::Vec3& value)
    {
        const auto length_squared = value.x * value.x + value.y * value.y + value.z * value.z;
        if (length_squared <= 0.0F)
            return tbx::Vec3(0.0F, 0.0F, 0.0F);

        return value * (1.0F / std::sqrt(length_squared));
    }
}
