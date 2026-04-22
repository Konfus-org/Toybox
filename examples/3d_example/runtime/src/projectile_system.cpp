#include "projectile_system.h"
#include "tbx/core/systems/assets/builtin_assets.h"
#include "tbx/core/systems/graphics/light.h"
#include "tbx/core/systems/math/transform.h"
#include "tbx/core/systems/physics/collider.h"
#include "tbx/core/systems/physics/physics.h"
#include <cmath>
#include <string>
#include <utility>

namespace three_d_example
{
    ProjectileSystem::ProjectileSystem(
        tbx::EntityRegistry& entity_registry,
        std::function<tbx::Entity()> camera_provider)
    {
        _entity_registry = &entity_registry;
        _camera_provider = std::move(camera_provider);
    }

    ProjectileSystem::~ProjectileSystem()
    {
        for (auto& projectile : _active_projectiles)
        {
            if (projectile.get_id().is_valid())
                projectile.destroy();
        }

        _entity_registry = nullptr;
        _camera_provider = {};
        _active_projectiles.clear();
        _active_projectile_lifetimes.clear();
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

    void ProjectileSystem::spawn_projectile()
    {
        if (_entity_registry == nullptr || !_camera_provider)
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
            auto oldest_projectile = _active_projectiles.front();
            if (oldest_projectile.get_id().is_valid())
                oldest_projectile.destroy();

            _active_projectiles.erase(_active_projectiles.begin());
            _active_projectile_lifetimes.erase(_active_projectile_lifetimes.begin());
        }

        constexpr auto projectile_visual_scale = 0.35F;
        auto projectile = tbx::Entity(projectile_name, *_entity_registry);
        projectile.add_component<tbx::MaterialInstance>(create_projectile_material());
        projectile.add_component<tbx::DynamicMesh>(_projectile_mesh);
        auto projectile_light = tbx::PointLight(tbx::Color(1.0F, 0.95F, 0.6F, 1.0F), 2.75F, 4.5F);
        projectile_light.shadows_enabled = false;
        projectile.add_component<tbx::PointLight>(projectile_light);
        projectile.add_component<tbx::Transform>(
            spawn_position,
            camera_world_transform.rotation,
            tbx::Vec3(projectile_visual_scale, projectile_visual_scale, projectile_visual_scale));
        projectile.add_component<tbx::SphereCollider>(projectile_visual_scale / 2.0F);
        projectile.add_component<tbx::Physics>(tbx::Physics {
            .mass = 0.2F,
            .linear_velocity = shot_direction * _projectile_speed,
            .friction = 0.2F,
            .restitution = 0.1F,
            .linear_damping = 0.02F,
            .angular_damping = 0.02F,
            .is_sleep_enabled = true,
        });

        _active_projectiles.push_back(projectile);
        _active_projectile_lifetimes.push_back(_projectile_lifetime_seconds);
    }

    void ProjectileSystem::update_projectiles(const tbx::DeltaTime& dt)
    {
        if (_active_projectiles.empty())
            return;

        const auto delta_seconds = dt.seconds;
        auto projectile_index = size {0U};
        while (projectile_index < _active_projectiles.size())
        {
            _active_projectile_lifetimes[projectile_index] -= delta_seconds;
            auto& projectile = _active_projectiles[projectile_index];

            const auto is_expired = _active_projectile_lifetimes[projectile_index] <= 0.0;
            const auto is_invalid = !projectile.get_id().is_valid();
            if (!is_expired && !is_invalid)
            {
                ++projectile_index;
                continue;
            }

            if (!is_invalid)
                projectile.destroy();

            const auto last_index = _active_projectiles.size() - 1U;
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

    tbx::MaterialInstance ProjectileSystem::create_projectile_material() const
    {
        auto material = tbx::MaterialInstance(tbx::PbrMaterial::HANDLE);
        material.set_parameter(tbx::PbrMaterial::COLOR, tbx::Color(1.0F, 0.92F, 0.15F, 1.0F));
        material.set_parameter(tbx::PbrMaterial::EMISSIVE, tbx::Color(1.0F, 0.82F, 0.12F, 1.0F));
        material.set_parameter(tbx::PbrMaterial::EMISSIVE_STRENGTH, 1.75F);
        return material;
    }
}
