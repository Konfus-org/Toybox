#pragma once
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/graphics/material.h"
#include "tbx/systems/graphics/mesh.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/typedefs.h"
#include <functional>
#include <memory>
#include <vector>


namespace three_d_example
{
    class ProjectileSystem final
    {
      public:
        ProjectileSystem(
            tbx::EntityRegistry& entity_registry,
            std::function<tbx::Entity()> camera_provider);
        ~ProjectileSystem();

        ProjectileSystem(const ProjectileSystem&) = delete;
        ProjectileSystem(ProjectileSystem&&) = delete;
        ProjectileSystem& operator=(const ProjectileSystem&) = delete;
        ProjectileSystem& operator=(ProjectileSystem&&) = delete;

        void update(const tbx::DeltaTime& dt);
        void request_spawn();

      private:
        void spawn_projectile();
        void update_projectiles(const tbx::DeltaTime& dt);
        tbx::MaterialInstance create_projectile_material() const;

      private:
        tbx::EntityRegistry* _entity_registry = nullptr;
        std::function<tbx::Entity()> _camera_provider = {};
        std::shared_ptr<tbx::Mesh> _projectile_mesh = std::make_shared<tbx::Mesh>(tbx::sphere);
        float _projectile_spawn_distance = 1.35F;
        float _projectile_speed = 26.0F;
        double _projectile_lifetime_seconds = 8.0;
        size _max_active_projectiles = 192U;
        bool _is_spawn_requested = false;
        size _spawned_projectile_count = 0U;
        std::vector<tbx::Entity> _active_projectiles = {};
        std::vector<double> _active_projectile_lifetimes = {};
    };
}
