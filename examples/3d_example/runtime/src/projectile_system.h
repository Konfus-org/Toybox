#pragma once
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/handle.h"
#include "tbx/types/typedefs.h"
#include <functional>
#include <memory>
#include <vector>

namespace three_d_example
{
    /// @brief
    /// Purpose: Tracks one runtime projectile and its remaining lifetime.
    struct ProjectileInstance final
    {
        tbx::Entity entity = {};
        double remaining_lifetime_seconds = 0.0;
    };

    /// @brief
    /// Purpose: Spawns, moves, and expires camera-fired runtime projectiles.
    /// @details
    /// Ownership: Does not own the world. Runtime projectiles are destroyed before the system
    /// releases its world reference.
    class ProjectileSystem final
    {
      public:
        ProjectileSystem(
            std::weak_ptr<tbx::World> world,
            std::function<tbx::Entity()> camera_provider);
        ~ProjectileSystem();

        ProjectileSystem(const ProjectileSystem&) = delete;
        ProjectileSystem(ProjectileSystem&&) = delete;
        ProjectileSystem& operator=(const ProjectileSystem&) = delete;
        ProjectileSystem& operator=(ProjectileSystem&&) = delete;

        void update(const tbx::DeltaTime& dt);
        void request_spawn();

      private:
        void destroy_projectile(tbx::World& world, tbx::Entity& projectile) const;
        void remove_projectile_at(size projectile_index);
        void spawn_projectile();
        void update_projectiles(const tbx::DeltaTime& dt);
        tbx::MaterialInstance create_projectile_material() const;

      private:
        std::weak_ptr<tbx::World> _world = {};
        std::function<tbx::Entity()> _camera_provider = {};
        tbx::MaterialInstance _projectile_material = {};
        tbx::Handle _projectile_model = {};
        float _projectile_spawn_distance = 1.35F;
        float _projectile_speed = 26.0F;
        double _projectile_lifetime_seconds = 8.0;
        size _max_active_projectiles = 192U;
        bool _is_spawn_requested = false;
        size _spawned_projectile_count = 0U;
        std::vector<ProjectileInstance> _active_projectiles = {};
    };
}
