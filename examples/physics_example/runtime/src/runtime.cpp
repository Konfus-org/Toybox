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

namespace physics_example
{
    using namespace tbx;
    void PhysicsExampleRuntimePlugin::on_attach(IPluginHost& host)
    {
        auto& ent_registry = host.get_entity_registry();
        auto& input_manager = host.get_input_manager();

        _sun = Entity("Light", ent_registry);
        _sun.add_component<DirectionalLight>(Color::WHITE, 1.0F, 0.15F);
        _sun.add_component<Transform>(Vec3(0), to_radians(Vec3(-45.0F, 45.0F, 0.0F)), Vec3(1));

        auto camera = Entity("Camera", ent_registry);
        camera.add_component<Camera>();
        camera.add_component<Transform>(
            Vec3(0.0F, 2.01F, 11.0F),
            Quat(Vec3(to_radians(-8.0F), 0.0F, 0.0F)),
            Vec3(1.0F));

        _camera_controller.initialize(
            camera,
            input_manager,
            example_common::FreeLookCameraControllerSettings {
                .initial_yaw = 0.0F,
                .initial_pitch = to_radians(-8.0F),
                .move_speed = 6.0F,
                .look_sensitivity = 0.0025F});

        _room.create(
            ent_registry,
            example_common::RoomSettings {
                .center = Vec3(0.0F, 0.0F, 0.0F),
                .include_colliders = true,
            });

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
                            [entity_registry =
                                 std::ref(ent_registry)](const ColliderOverlapEvent& overlap_event)
                            {
                                TBX_TRACE_INFO(
                                    "Trigger overlap begin: {} with {}.",
                                    entity_registry.get()
                                        .get(overlap_event.trigger_entity_id)
                                        .get_name(),
                                    entity_registry.get()
                                        .get(overlap_event.overlapped_entity_id)
                                        .get_name());
                            },
                        },
                    .overlap_end_callbacks =
                        {
                            [entity_registry =
                                 std::ref(ent_registry)](const ColliderOverlapEvent& overlap_event)
                            {
                                TBX_TRACE_INFO(
                                    "Trigger overlap end: {} with {}.",
                                    entity_registry.get()
                                        .get(overlap_event.trigger_entity_id)
                                        .get_name(),
                                    entity_registry.get()
                                        .get(overlap_event.overlapped_entity_id)
                                        .get_name());
                            },
                        },
                },
        });

        auto& camera_scheme = _camera_controller.get_input_scheme();
        camera_scheme.add_action(InputAction(
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
                            const auto& camera = _camera_controller.get_camera();
                            if (!camera.get_id().is_valid())
                                return;

                            Transform camera_world_transform = get_world_space_transform(camera);
                            Vec3 direction = normalize(
                                camera_world_transform.rotation * Vec3(0.0F, 0.0F, -1.0F));

                            Raycast raycast = Raycast {
                                .origin = camera_world_transform.position,
                                .direction = direction,
                                .max_distance = 60.0F,
                                .ignore_entity = true,
                                .ignored_entity_id = camera.get_id(),
                            };

                            RaycastResult raycast_result = raycast.cast();
                            if (!raycast_result)
                            {
                                TBX_TRACE_INFO("Raycast missed.");
                                return;
                            }

                            TBX_TRACE_INFO(
                                "Raycast hit entity {} at ({:.2f}, {:.2f}, {:.2f}).",
                                get_host()
                                    .get_entity_registry()
                                    .get(raycast_result.hit_entity_id)
                                    .get_name(),
                                raycast_result.hit_position.x,
                                raycast_result.hit_position.y,
                                raycast_result.hit_position.z);
                        },
                    },
            }));
        camera_scheme.add_action(InputAction(
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
            }));
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

        _camera_controller.shutdown(get_host().get_input_manager());

        _room.destroy();
    }

    void PhysicsExampleRuntimePlugin::on_update(const DeltaTime& dt)
    {
        _camera_controller.update(dt);

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
        const auto& camera = _camera_controller.get_camera();
        if (!camera.get_id().is_valid())
            return;

        const auto camera_world_transform = get_world_space_transform(camera);
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

        auto projectile = Entity(projectile_name, get_host().get_entity_registry());
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
