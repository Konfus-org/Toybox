#pragma once
#include "tbx/ecs/entity.h"
#include "tbx/examples/free_look_camera_controller.h"
#include "tbx/examples/room.h"
#include "tbx/graphics/mesh.h"
#include "tbx/plugin_api/plugin.h"
#include <cstddef>
#include <memory>
#include <vector>

namespace physics_example
{
    using namespace tbx;
    /// <summary>
    /// Purpose: Demonstrates runtime rigid-body simulation and transform sync behaviors.
    /// </summary>
    /// <remarks>
    /// Ownership: Holds non-owning references to host ECS services.
    /// Thread Safety: Must run on the main thread.
    /// </remarks>
    class PhysicsExampleRuntimePlugin final : public Plugin
    {
      public:
        void on_attach(IPluginHost& host) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;

      private:
        void update_projectiles(const DeltaTime& dt);
        void spawn_projectile();

      private:
        example_common::FreeLookCameraController _camera_controller = {};
        example_common::Room _room = {};
        Entity _sun = {};

        float _projectile_spawn_distance = 1.35F;
        float _projectile_speed = 26.0F;
        double _projectile_lifetime_seconds = 8.0;
        size_t _max_active_projectiles = 192U;

        bool _is_shoot_requested = false;

        size_t _spawned_projectile_count = 0U;
        std::shared_ptr<Mesh> _projectile_mesh = std::make_shared<Mesh>(sphere);
        std::vector<Entity> _active_projectiles = {};
        std::vector<double> _active_projectile_lifetimes = {};
    };
}
