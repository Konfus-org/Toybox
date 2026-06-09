#include "player_controller.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/assets/builtin_assets.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/components/collider.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/rigidbody.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/raycast.h"
#include "tbx/types/trig.h"
#include <cmath>

namespace tbx_example
{
    static void remove_player_input_actions(tbx::InputScheme& scheme)
    {
        scheme.remove_action("Look");
        scheme.remove_action("Move");
        scheme.remove_action("Raycast");
        scheme.remove_action("Shoot");
        scheme.remove_action("VerticalMove");
    }

    void PlayerController::on_destroy()
    {
        if (auto input_manager = input.lock())
        {
            if (auto scheme = input_manager->get_scheme(_scheme_name); scheme.has_value())
                remove_player_input_actions(scheme->get());
            input_manager->set_mouse_lock_mode(tbx::MouseLockMode::UNLOCKED);
        }

        if (!_world.expired())
        {
            for (auto& projectile : _active_projectiles)
                destroy_projectile(projectile.entity);
        }

        _active_projectiles.clear();
        _camera_entity = {};
        _world = {};
        reset_input_state();
    }

    void PlayerController::on_start()
    {
        _world = get_world_ptr();
        auto world = _world.lock();
        auto input_manager = input.lock();
        if (!world || !input_manager)
        {
            TBX_TRACE_WARNING(
                "PlayerController could not start because the world or input service is missing.");
            return;
        }

        _camera_entity = world->get(camera_entity);
        if (!_camera_entity.get_id().is_valid())
        {
            TBX_TRACE_WARNING("PlayerController could not find the authored camera entity.");
            return;
        }

        _yaw = initial_yaw;
        _pitch = initial_pitch;
        _projectile_material = create_projectile_material();
        _projectile_model = tbx::SphereModel::HANDLE;
        _active_projectiles.reserve(_max_active_projectiles);

        if (auto& character = get_entity(); character.has_component<tbx::Transform>())
            character.get_component<tbx::Transform>().rotation =
                tbx::Quat(tbx::Vec3(0.0F, _yaw, 0.0F));

        if (_camera_entity.has_component<tbx::Transform>())
            _camera_entity.get_component<tbx::Transform>().rotation =
                tbx::Quat(tbx::Vec3(_pitch, 0.0F, 0.0F));

        setup_input();
    }

    void PlayerController::on_update(const tbx::DeltaTime& dt)
    {
        update_camera(dt);
        update_actions();
        update_projectiles(dt);
    }

    tbx::InputAction PlayerController::create_look_action()
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

    tbx::InputAction PlayerController::create_move_action()
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

    tbx::InputAction PlayerController::create_raycast_action()
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
                            _raycast_requested = true;
                        },
                    },
            });
    }

    tbx::InputAction PlayerController::create_shoot_action()
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
                            _shoot_requested = true;
                        },
                    },
            });
    }

    tbx::InputAction PlayerController::create_vertical_move_action()
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

    tbx::MaterialInstance PlayerController::create_projectile_material() const
    {
        auto material = tbx::MaterialInstance(tbx::PbrMaterial::HANDLE);
        auto config = tbx::MaterialConfig();
        config.shadow_mode = tbx::ShadowMode::OFF;
        material.set_config(config);
        material.set_parameter(
            tbx::PbrMaterial::ALBEDO_COLOR,
            tbx::Color(1.0F, 0.92F, 0.15F, 1.0F));
        material.set_parameter(
            tbx::PbrMaterial::EMISSIVE_COLOR,
            tbx::Color(1.75F, 1.435F, 0.21F, 1.0F));
        return material;
    }

    tbx::Vec3 PlayerController::get_shot_direction(const tbx::Transform& camera_transform) const
    {
        auto direction = camera_transform.rotation * tbx::Vec3(0.0F, 0.0F, -1.0F);
        direction = normalize_or_zero(direction);
        return direction.x == 0.0F && direction.y == 0.0F && direction.z == 0.0F
                   ? tbx::Vec3(0.0F, 0.0F, -1.0F)
                   : direction;
    }

    void PlayerController::cast_raycast() const
    {
        auto world = _world.lock();
        auto physics_system = physics.lock();
        if (!world || !physics_system || !_camera_entity.get_id().is_valid()
            || !_camera_entity.has_component<tbx::Transform>())
            return;

        const auto camera_world_transform =
            _camera_entity.get_component<tbx::Transform>().to_world_space(_camera_entity);
        auto raycast = tbx::RaycastQuery {
            .origin = camera_world_transform.position,
            .direction = get_shot_direction(camera_world_transform),
            .max_distance = 60.0F,
            .ignore_entity = true,
            .ignored_entity_id = _camera_entity.get_id(),
        };

        const auto raycast_result = physics_system->raycast(raycast);
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

    void PlayerController::destroy_projectile(tbx::Entity& projectile) const
    {
        auto world = _world.lock();
        if (world && projectile.get_id().is_valid() && world->has(projectile.get_id()))
            world->destroy(projectile);
    }

    void PlayerController::remove_projectile_at(size projectile_index)
    {
        const auto last_index = _active_projectiles.size() - 1U;
        if (projectile_index != last_index)
            _active_projectiles[projectile_index] = _active_projectiles[last_index];

        _active_projectiles.pop_back();
    }

    void PlayerController::reset_input_state()
    {
        _look_delta = tbx::Vec2(0.0F, 0.0F);
        _move_axis = tbx::Vec2(0.0F, 0.0F);
        _raycast_requested = false;
        _shoot_requested = false;
        _vertical_axis = tbx::Vec2(0.0F, 0.0F);
    }

    void PlayerController::setup_input()
    {
        auto input_manager = input.lock();
        if (!input_manager)
            return;

        if (!input_manager->get_scheme(_scheme_name).has_value())
            input_manager->add_scheme(tbx::InputScheme(_scheme_name));

        auto scheme = input_manager->get_scheme(_scheme_name);
        if (!scheme.has_value())
            return;

        auto actions = std::vector<tbx::InputAction> {
            create_look_action(),
            create_move_action(),
            create_raycast_action(),
            create_shoot_action(),
            create_vertical_move_action(),
        };

        remove_player_input_actions(scheme->get());
        for (const auto& action : actions)
            scheme->get().add_action(action);

        input_manager->activate_scheme(_scheme_name);
        input_manager->set_mouse_lock_mode(tbx::MouseLockMode::RELATIVE);
    }

    void PlayerController::spawn_projectile()
    {
        auto world = _world.lock();
        if (!world || !_camera_entity.get_id().is_valid()
            || !_camera_entity.has_component<tbx::Transform>())
            return;

        const auto camera_world_transform =
            _camera_entity.get_component<tbx::Transform>().to_world_space(_camera_entity);
        const auto shot_direction = get_shot_direction(camera_world_transform);
        const auto spawn_position =
            camera_world_transform.position + (shot_direction * _projectile_spawn_distance);
        auto projectile_name =
            std::string("Projectile_") + std::to_string(_spawned_projectile_count);
        _spawned_projectile_count += 1U;

        while (_active_projectiles.size() >= _max_active_projectiles)
        {
            destroy_projectile(_active_projectiles.front().entity);
            remove_projectile_at(0U);
        }

        constexpr auto projectile_visual_scale = 0.35F;
        auto projectile = world->create_entity(projectile_name);
        projectile.add_component<tbx::MaterialInstance>(_projectile_material);
        projectile.add_component<tbx::StaticMesh>(_projectile_model);
        projectile.add_component<tbx::Transform>(
            spawn_position,
            camera_world_transform.rotation,
            tbx::Vec3(projectile_visual_scale, projectile_visual_scale, projectile_visual_scale));
        projectile.add_component<tbx::SphereCollider>(projectile_visual_scale);
        auto rigidbody = tbx::Rigidbody {};
        rigidbody.mass = 0.2F;
        rigidbody.linear_velocity = shot_direction * _projectile_speed;
        rigidbody.friction = 0.2F;
        rigidbody.restitution = 0.1F;
        rigidbody.linear_damping = 0.02F;
        rigidbody.angular_damping = 0.02F;
        rigidbody.is_sleep_enabled = true;
        projectile.add_component<tbx::Rigidbody>(rigidbody);

        _active_projectiles.push_back(
            ProjectileInstance {
                .entity = projectile,
                .remaining_lifetime_seconds = _projectile_lifetime_seconds,
            });
    }

    void PlayerController::update_actions()
    {
        if (_raycast_requested)
        {
            _raycast_requested = false;
            cast_raycast();
        }

        if (_shoot_requested)
        {
            _shoot_requested = false;
            spawn_projectile();
        }
    }

    void PlayerController::update_camera(const tbx::DeltaTime& dt)
    {
        auto& character = get_entity();
        if (!character.get_id().is_valid() || !_camera_entity.get_id().is_valid())
            return;
        if (!character.has_component<tbx::Transform>()
            || !_camera_entity.has_component<tbx::Transform>())
        {
            return;
        }

        _yaw -= _look_delta.x * look_sensitivity;
        _pitch -= _look_delta.y * look_sensitivity;

        const auto max_pitch = tbx::to_radians(89.0F);
        if (_pitch > max_pitch)
            _pitch = max_pitch;
        if (_pitch < -max_pitch)
            _pitch = -max_pitch;

        const auto yaw_rotation = tbx::normalize(tbx::Quat(tbx::Vec3(0.0F, _yaw, 0.0F)));
        const auto pitch_rotation = tbx::normalize(tbx::Quat(tbx::Vec3(_pitch, 0.0F, 0.0F)));

        auto character_transform = character.get_component<tbx::Transform>();
        auto forward = normalize_or_zero(yaw_rotation * tbx::Vec3(0.0F, 0.0F, -1.0F));
        auto right = normalize_or_zero(yaw_rotation * tbx::Vec3(1.0F, 0.0F, 0.0F));
        forward.y = 0.0F;
        right.y = 0.0F;
        forward = normalize_or_zero(forward);
        right = normalize_or_zero(right);

        const auto move_direction =
            normalize_or_zero((forward * _move_axis.y) + (right * _move_axis.x));
        if (move_direction.x != 0.0F || move_direction.y != 0.0F || move_direction.z != 0.0F)
        {
            character_transform.position +=
                move_direction * move_speed * static_cast<float>(dt.seconds);
        }

        character_transform.position +=
            tbx::Vec3(0.0F, _vertical_axis.y, 0.0F) * move_speed * static_cast<float>(dt.seconds);
        character_transform.rotation = yaw_rotation;

        character.get_component<tbx::Transform>() = character_transform;
        _camera_entity.get_component<tbx::Transform>().rotation = pitch_rotation;
    }

    void PlayerController::update_projectiles(const tbx::DeltaTime& dt)
    {
        if (_active_projectiles.empty())
            return;

        auto world = _world.lock();
        const auto delta_seconds = dt.seconds;
        auto projectile_index = size {0U};
        while (projectile_index < _active_projectiles.size())
        {
            auto& projectile = _active_projectiles[projectile_index];
            projectile.remaining_lifetime_seconds -= delta_seconds;

            const auto is_expired = projectile.remaining_lifetime_seconds <= 0.0;
            const auto is_invalid = !projectile.entity.get_id().is_valid();
            if (!is_expired && !is_invalid)
            {
                ++projectile_index;
                continue;
            }

            if (world && !is_invalid)
                destroy_projectile(projectile.entity);

            remove_projectile_at(projectile_index);
        }
    }

    tbx::Vec3 PlayerController::normalize_or_zero(const tbx::Vec3& value)
    {
        const auto length_squared = value.x * value.x + value.y * value.y + value.z * value.z;
        if (length_squared <= 0.0F)
            return tbx::Vec3(0.0F, 0.0F, 0.0F);

        return value * (1.0F / std::sqrt(length_squared));
    }
}
