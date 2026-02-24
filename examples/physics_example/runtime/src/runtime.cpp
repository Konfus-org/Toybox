#include "runtime.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/light.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/renderer.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"
#include "tbx/physics/collider.h"
#include "tbx/physics/physics.h"
#include "tbx/physics/raycast.h"
#include <cmath>
#include <string>

namespace tbx::examples
{
    void PhysicsExampleRuntimePlugin::on_attach(IPluginHost& host)
    {
        _ent_registry = &host.get_entity_registry();
        _input_manager = &host.get_input_manager();

        auto& ent_registry = *_ent_registry;

        _sun = Entity("Light", ent_registry);
        _sun.add_component<DirectionalLight>(Color::WHITE, 1.0F, 0.15F);
        _sun.add_component<Transform>(Vec3(0), to_radians(Vec3(-45.0F, 45.0F, 0.0F)), Vec3(1));

        _camera_yaw = 0.0F;
        _camera_pitch = to_radians(-8.0F);
        _camera = Entity("Camera", ent_registry);
        _camera.add_component<Camera>();
        _camera.add_component<Transform>(
            Vec3(0.0F, 2.01F, 11.0F),
            Quat(Vec3(_camera_pitch, _camera_yaw, 0.0F)),
            Vec3(1.0F));

        auto ground_ent = Entity("Ground", ent_registry);
        ground_ent.add_component<Renderer>();
        ground_ent.add_component<DynamicMesh>(quad);
        ground_ent.add_component<MeshCollider>();
        ground_ent.add_component<Transform>(
            Vec3(0.0F, 0.0f, 0.0F),
            to_radians(Vec3(-90.0F, 0.0F, 0.0F)),
            Vec3(40.0F, 40.0F, 1.0F));

        auto wallf_ent = Entity("Wall_F", ent_registry);
        wallf_ent.add_component<Renderer>();
        wallf_ent.add_component<DynamicMesh>(cube);
        wallf_ent.add_component<CubeCollider>(Vec3(22.5F, 5.5F, 0.75F));
        wallf_ent.add_component<Transform>(
            Vec3(0.0F, 5.0F, -20.0F),
            Quat(Vec3(0.0F, 0.0F, 0.0F)),
            Vec3(45.0F, 11.0F, 1.5F));

        auto walll_ent = Entity("Wall_L", ent_registry);
        walll_ent.add_component<Renderer>();
        walll_ent.add_component<DynamicMesh>(cube);
        walll_ent.add_component<CubeCollider>(Vec3(0.75F, 5.5F, 22.5F));
        walll_ent.add_component<Transform>(
            Vec3(-20.0F, 5.0F, 0.0F),
            Quat(Vec3(0.0F, 0.0F, 0.0F)),
            Vec3(1.5F, 11.0F, 45.0F));

        auto wallr_ent = Entity("Wall_R", ent_registry);
        wallr_ent.add_component<Renderer>();
        wallr_ent.add_component<DynamicMesh>(cube);
        wallr_ent.add_component<CubeCollider>(Vec3(0.75F, 5.5F, 22.5F));
        wallr_ent.add_component<Transform>(
            Vec3(20.0F, 5.0F, 0.0F),
            Quat(Vec3(0.0F, 0.0F, 0.0F)),
            Vec3(1.5F, 11.0F, 45.0F));

        auto wallb_ent = Entity("Wall_B", ent_registry);
        wallb_ent.add_component<Renderer>();
        wallb_ent.add_component<DynamicMesh>(cube);
        wallb_ent.add_component<CubeCollider>(Vec3(22.5F, 5.5F, 0.75F));
        wallb_ent.add_component<Transform>(
            Vec3(0.0F, 5.0F, 20.0F),
            Quat(Vec3(0.0F, 0.0F, 0.0F)),
            Vec3(45.0F, 11.0F, 1.5F));

        auto falling_sphere = Entity("FallingSphere", ent_registry);
        falling_sphere.add_component<Renderer>(MaterialInstance {
            .parameters = {{"color", Color::RED}},
        });
        falling_sphere.add_component<DynamicMesh>(sphere);
        falling_sphere.add_component<Transform>(Vec3(0.0F, 6.0F, -5.2F));
        falling_sphere.add_component<SphereCollider>(0.5F);
        falling_sphere.add_component<Physics>();

        auto falling_cube = Entity("FallingCube", ent_registry);
        falling_cube.add_component<Renderer>(MaterialInstance {
            .parameters = {{"color", Color::BLUE}},
        });
        falling_cube.add_component<DynamicMesh>(cube);
        falling_cube.add_component<Transform>(Vec3(0.0F, 10.0F, -4.9F));
        falling_cube.add_component<CubeCollider>(Vec3(0.5F, 0.5F, 0.5F));
        falling_cube.add_component<Physics>();

        auto trigger_zone = Entity("TriggerZone", ent_registry);
        trigger_zone.add_component<Renderer>(MaterialInstance {
            .parameters = {{"color", Color::GREEN}},
        });
        trigger_zone.add_component<DynamicMesh>(cube);
        trigger_zone.add_component<Transform>(
            Vec3(0.0F, 2.5F, -5.0F),
            Quat(Vec3(0.0F, 0.0F, 0.0F)),
            Vec3(3.0F, 3.0F, 3.0F));
        trigger_zone.add_component<CubeCollider>(CubeCollider {
            .half_extents = Vec3(1.5F, 1.5F, 1.5F),
            .trigger =
                ColliderTrigger {
                    .is_trigger_only = true,
                    .is_overlap_enabled = true,
                    .overlap_begin_callbacks =
                        {
                            [](const ColliderOverlapEvent& overlap_event)
                            {
                                TBX_TRACE_INFO(
                                    "Trigger overlap begin: {} with {}.",
                                    to_string(overlap_event.trigger_entity_id),
                                    to_string(overlap_event.overlapped_entity_id));
                            },
                        },
                    .overlap_end_callbacks =
                        {
                            [](const ColliderOverlapEvent& overlap_event)
                            {
                                TBX_TRACE_INFO(
                                    "Trigger overlap end: {} with {}.",
                                    to_string(overlap_event.trigger_entity_id),
                                    to_string(overlap_event.overlapped_entity_id));
                            },
                        },
                },
        });

        auto camera_scheme = InputScheme(
            _camera_scheme_name,
            {
                InputAction(
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
                                    Vec2 move_axis = Vec2(0.0F, 0.0F);
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
                    }),
                InputAction(
                    "UpDown",
                    InputActionValueType::VECTOR2,
                    InputActionConstruction {
                        .bindings =
                            {
                                InputBinding {
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
                                [this](const InputAction& action)
                                {
                                    Vec2 move_axis = Vec2(0.0F, 0.0F);
                                    if (action.try_get_value_as<Vec2>(move_axis))
                                        _updown_axis = move_axis;
                                },
                            },
                        .on_cancelled_callbacks =
                            {
                                [this](const InputAction&)
                                {
                                    _updown_axis = Vec2(0.0F, 0.0F);
                                },
                            },
                    }),
                InputAction(
                    "Raycast",
                    InputActionValueType::BUTTON,
                    InputActionConstruction {
                        .bindings =
                            {
                                InputBinding {
                                    .control =
                                        MouseButtonInputControl {
                                            .button = InputMouseButton::LEFT,
                                        },
                                    .scale = 1.0F,
                                },
                            },
                        .on_start_callbacks =
                            {
                                [this](const InputAction&)
                                {
                                    if (!_camera.get_id().is_valid())
                                        return;

                                    Transform camera_world_transform =
                                        get_world_space_transform(_camera);
                                    Vec3 direction = normalize(
                                        camera_world_transform.rotation * Vec3(0.0F, 0.0F, -1.0F));

                                    Raycast raycast = Raycast {
                                        .origin = camera_world_transform.position,
                                        .direction = direction,
                                        .max_distance = 60.0F,
                                        .ignore_entity = true,
                                        .ignored_entity_id = _camera.get_id(),
                                    };

                                    RaycastResult raycast_result = raycast.cast();
                                    if (!raycast_result)
                                    {
                                        TBX_TRACE_INFO("Raycast missed.");
                                        return;
                                    }

                                    TBX_TRACE_INFO(
                                        "Raycast hit entity {} at ({:.2f}, {:.2f}, {:.2f}).",
                                        to_string(raycast_result.hit_entity_id),
                                        raycast_result.hit_position.x,
                                        raycast_result.hit_position.y,
                                        raycast_result.hit_position.z);
                                },
                            },
                    }),
                InputAction(
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
                                    Vec2 look_delta = Vec2(0.0F, 0.0F);
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
                    }),
                InputAction(
                    "Shoot",
                    InputActionValueType::BUTTON,
                    InputActionConstruction {
                        .bindings =
                            {
                                InputBinding {
                                    .control =
                                        MouseButtonInputControl {
                                            .button = InputMouseButton::RIGHT,
                                        },
                                    .scale = 1.0F,
                                },
                            },
                        .on_start_callbacks =
                            {
                                [this](const InputAction&)
                                {
                                    _is_shoot_requested = true;
                                },
                            },
                    }),
            });

        _input_manager->add_scheme(camera_scheme);
        _input_manager->activate_scheme(_camera_scheme_name);
        _input_manager->set_mouse_lock_mode(MouseLockMode::RELATIVE);
    }

    void PhysicsExampleRuntimePlugin::on_detach()
    {
        for (auto& projectile : _active_projectiles)
        {
            if (projectile.get_id().is_valid())
                projectile.destroy();
        }
        _active_projectiles.clear();
        _active_projectile_lifetimes.clear();

        if (_input_manager)
            _input_manager->remove_scheme(_camera_scheme_name);

        _input_manager = nullptr;
        _ent_registry = nullptr;
    }

    void PhysicsExampleRuntimePlugin::on_update(const DeltaTime& dt)
    {
        auto& light_transform = _sun.get_component<Transform>();
        const float yaw_rate = 0.5F;
        float angle = yaw_rate * static_cast<float>(dt.seconds);
        auto light_delta = Quat({0.0F, angle, 0.0F});
        light_transform.rotation = normalize(light_delta * light_transform.rotation);

        auto& camera_transform = _camera.get_component<Transform>();

        _camera_yaw -= _look_delta.x * _camera_look_sensitivity;
        _camera_pitch -= _look_delta.y * _camera_look_sensitivity;

        const float max_pitch = to_radians(89.0F);
        if (_camera_pitch > max_pitch)
            _camera_pitch = max_pitch;
        if (_camera_pitch < -max_pitch)
            _camera_pitch = -max_pitch;

        const Quat yaw_rotation = normalize(Quat(Vec3(0.0F, _camera_yaw, 0.0F)));
        const Quat pitch_rotation = normalize(Quat(Vec3(_camera_pitch, 0.0F, 0.0F)));

        const float right_axis = _move_axis.x;
        const float forward_axis = _move_axis.y;

        Vec3 forward = yaw_rotation * Vec3(0.0F, 0.0F, -1.0F);
        Vec3 right = yaw_rotation * Vec3(1.0F, 0.0F, 0.0F);
        forward.y = 0.0F;
        right.y = 0.0F;

        const float forward_length_squared =
            forward.x * forward.x + forward.y * forward.y + forward.z * forward.z;
        if (forward_length_squared > 0.0F)
            forward *= 1.0F / std::sqrt(forward_length_squared);

        const float right_length_squared =
            right.x * right.x + right.y * right.y + right.z * right.z;
        if (right_length_squared > 0.0F)
            right *= 1.0F / std::sqrt(right_length_squared);

        Vec3 move = forward * forward_axis + right * right_axis;
        const float move_length_squared = move.x * move.x + move.y * move.y + move.z * move.z;
        if (move_length_squared > 0.0F)
        {
            const float inverse_length = 1.0F / std::sqrt(move_length_squared);
            move *= inverse_length;
            camera_transform.position += move * _camera_move_speed * static_cast<float>(dt.seconds);
        }

        camera_transform.position += Vec3(_updown_axis.x, _updown_axis.y, 0.0F) * _camera_move_speed
                                     * static_cast<float>(dt.seconds);

        camera_transform.rotation = normalize(yaw_rotation * pitch_rotation);

        update_projectiles(dt);

        if (_is_shoot_requested)
        {
            spawn_projectile();
            _is_shoot_requested = false;
        }
    }

    void PhysicsExampleRuntimePlugin::update_projectiles(const DeltaTime& dt)
    {
        if (_active_projectiles.empty())
            return;

        const double delta_seconds = dt.seconds;
        size_t projectile_index = 0U;
        while (projectile_index < _active_projectiles.size())
        {
            _active_projectile_lifetimes[projectile_index] -= delta_seconds;
            auto& projectile = _active_projectiles[projectile_index];

            const bool is_expired = _active_projectile_lifetimes[projectile_index] <= 0.0;
            const bool is_invalid = !projectile.get_id().is_valid();
            if (!is_expired && !is_invalid)
            {
                ++projectile_index;
                continue;
            }

            if (!is_invalid)
                projectile.destroy();

            const size_t last_index = _active_projectiles.size() - 1U;
            if (projectile_index != last_index)
            {
                _active_projectiles[projectile_index] = _active_projectiles[last_index];
                _active_projectile_lifetimes[projectile_index] =
                    _active_projectile_lifetimes[last_index];
            }

            _active_projectiles.pop_back();
            _active_projectile_lifetimes.pop_back();
        }
    }

    void PhysicsExampleRuntimePlugin::spawn_projectile()
    {
        if (_ent_registry == nullptr)
            return;

        const auto camera_world_transform = get_world_space_transform(_camera);
        Vec3 shot_direction = camera_world_transform.rotation * Vec3(0.0F, 0.0F, -1.0F);
        const float direction_length_squared = shot_direction.x * shot_direction.x
                                               + shot_direction.y * shot_direction.y
                                               + shot_direction.z * shot_direction.z;
        if (direction_length_squared <= 0.000001F)
            shot_direction = Vec3(0.0F, 0.0F, -1.0F);
        else
            shot_direction *= 1.0F / std::sqrt(direction_length_squared);

        const Vec3 spawn_position =
            camera_world_transform.position + (shot_direction * _projectile_spawn_distance);
        auto projectile_name =
            std::string("Projectile_") + std::to_string(_spawned_projectile_count);
        _spawned_projectile_count += 1U;

        while (_active_projectiles.size() >= _max_active_projectiles)
        {
            auto oldest_projectile = _active_projectiles.front();
            if (oldest_projectile.get_id().is_valid())
                oldest_projectile.destroy();

            _active_projectiles.erase(_active_projectiles.begin());
            _active_projectile_lifetimes.erase(_active_projectile_lifetimes.begin());
        }

        auto projectile = Entity(projectile_name, *_ent_registry);
        constexpr float projectile_visual_scale = 0.35F;
        projectile.add_component<Renderer>(MaterialInstance {
            .parameters = {{"color", Color::YELLOW}},
        });
        projectile.add_component<DynamicMesh>(_projectile_mesh);
        projectile.add_component<Transform>(
            spawn_position,
            camera_world_transform.rotation,
            Vec3(projectile_visual_scale, projectile_visual_scale, projectile_visual_scale));
        projectile.add_component<SphereCollider>(projectile_visual_scale / 2.0F);
        projectile.add_component<Physics>(Physics {
            .mass = 0.2F,
            .linear_velocity = shot_direction * _projectile_speed,
            .friction = 0.2F,
            .restitution = 0.1F,
            .default_linear_damping = 0.02F,
            .default_angular_damping = 0.02F,
            .is_sleep_enabled = true,
        });

        _active_projectiles.push_back(projectile);
        _active_projectile_lifetimes.push_back(_projectile_lifetime_seconds);
    }
}
