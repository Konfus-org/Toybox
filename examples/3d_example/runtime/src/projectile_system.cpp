#include "projectile_system.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/assets/builtin_assets.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/components/collider.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/rigidbody.h"
#include "tbx/types/components/transform.h"

namespace three_d_example
{
    ProjectileSystem::ProjectileSystem(
        std::weak_ptr<tbx::World> world,
        std::function<tbx::Entity()> camera_provider)
    {
        _world = world;
        _camera_provider = std::move(camera_provider);
        _projectile_material = create_projectile_material();
        _projectile_model = tbx::SphereModel::HANDLE;
    }

    ProjectileSystem::~ProjectileSystem()
    {
        auto world = _world.lock();
        for (auto& projectile : _active_projectiles)
        {
            if (world)
                destroy_projectile(*world, projectile.entity);
        }

        _world.reset();
        _camera_provider = {};
        _active_projectiles.clear();
        _is_spawn_requested = false;
        _spawned_projectile_count = 0U;
    }

    void ProjectileSystem::update(const tbx::DeltaTime& dt)
    {
        update_projectiles(dt);

        if (_is_spawn_requested)
        {
            spawn_projectile();
            _is_spawn_requested = false;
        }
    }

    void ProjectileSystem::request_spawn()
    {
        _is_spawn_requested = true;
    }

    void ProjectileSystem::destroy_projectile(tbx::World& world, tbx::Entity& projectile) const
    {
        if (projectile.get_id().is_valid() && world.has(projectile.get_id()))
            world.destroy(projectile);
    }

    void ProjectileSystem::remove_projectile_at(size projectile_index)
    {
        const auto last_index = _active_projectiles.size() - 1U;
        if (projectile_index != last_index)
            _active_projectiles[projectile_index] = _active_projectiles[last_index];

        _active_projectiles.pop_back();
    }

    void ProjectileSystem::spawn_projectile()
    {
        auto world = _world.lock();
        if (!world || !_camera_provider)
            return;

        const auto camera = _camera_provider();
        if (!camera.get_id().is_valid())
            return;

        const auto camera_world_transform = get_world_space_transform(camera);
        auto shot_direction = camera_world_transform.rotation * tbx::Vec3(0.0F, 0.0F, -1.0F);
        const auto direction_length_squared = shot_direction.x * shot_direction.x
                                              + shot_direction.y * shot_direction.y
                                              + shot_direction.z * shot_direction.z;
        if (direction_length_squared <= 0.000001F)
            shot_direction = tbx::Vec3(0.0F, 0.0F, -1.0F);
        else
            shot_direction *= 1.0F / std::sqrt(direction_length_squared);

        const auto spawn_position =
            camera_world_transform.position + (shot_direction * _projectile_spawn_distance);
        auto projectile_name =
            std::string("Projectile_") + std::to_string(_spawned_projectile_count);
        _spawned_projectile_count += 1U;

        while (_active_projectiles.size() >= _max_active_projectiles)
        {
            destroy_projectile(*world, _active_projectiles.front().entity);
            _active_projectiles.erase(_active_projectiles.begin());
        }

        // Runtime projectiles are visualized with the built-in sphere and simulated by physics.
        constexpr auto projectile_visual_scale = 0.35F;
        auto projectile = world->create_spatial_entity(projectile_name);
        projectile.add_component<tbx::MaterialInstance>(_projectile_material);
        projectile.add_component<tbx::StaticMesh>(_projectile_model);
        projectile.add_component<tbx::Transform>(
            spawn_position,
            camera_world_transform.rotation,
            tbx::Vec3(projectile_visual_scale, projectile_visual_scale, projectile_visual_scale));
        projectile.add_component<tbx::SphereCollider>(projectile_visual_scale / 2.0F);
        auto rigidbody = tbx::Rigidbody {};
        rigidbody.mass = 0.2F;
        rigidbody.linear_velocity = shot_direction * _projectile_speed;
        rigidbody.friction = 0.2F;
        rigidbody.restitution = 0.1F;
        rigidbody.linear_damping = 0.02F;
        rigidbody.angular_damping = 0.02F;
        rigidbody.is_sleep_enabled = true;
        projectile.add_component<tbx::Rigidbody>(rigidbody);
        world->update_chunk_membership();

        _active_projectiles.push_back(
            ProjectileInstance {
                .entity = projectile,
                .remaining_lifetime_seconds = _projectile_lifetime_seconds,
            });
    }

    void ProjectileSystem::update_projectiles(const tbx::DeltaTime& dt)
    {
        if (_active_projectiles.empty())
            return;

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

            if (!is_invalid)
            {
                auto world = _world.lock();
                if (world)
                    destroy_projectile(*world, projectile.entity);
            }

            remove_projectile_at(projectile_index);
        }
    }

    tbx::MaterialInstance ProjectileSystem::create_projectile_material() const
    {
        auto material = tbx::MaterialInstance(tbx::PbrMaterial::HANDLE);
        material.set_parameter(
            tbx::PbrMaterial::ALBEDO_COLOR,
            tbx::Color(1.0F, 0.92F, 0.15F, 1.0F));
        material.set_parameter(
            tbx::PbrMaterial::EMISSIVE_COLOR,
            tbx::Color(1.75F, 1.435F, 0.21F, 1.0F));
        return material;
    }
}
