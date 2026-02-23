#pragma once
#include "tbx/ecs/entity.h"
#include "tbx/input/input_manager.h"
#include "tbx/plugin_api/plugin.h"
#include <cstddef>
#include <string>
#include <vector>

namespace tbx::examples
{
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
        EntityRegistry* _ent_registry = nullptr;
        InputManager* _input_manager = nullptr;

        Entity _camera = {};
        Entity _sun = {};

        std::string _camera_scheme_name = "PhysicsExample.Camera";

        float _camera_yaw = 0.0F;
        float _camera_pitch = 0.0F;
        float _camera_move_speed = 6.0F;
        float _camera_look_sensitivity = 0.0025F;
        float _projectile_spawn_distance = 1.35F;
        float _projectile_speed = 26.0F;
        double _projectile_lifetime_seconds = 8.0;
        size_t _max_active_projectiles = 192U;

        Vec2 _move_axis = Vec2(0.0F, 0.0F);
        Vec2 _updown_axis = Vec2(0.0F, 0.0F);
        Vec2 _look_delta = Vec2(0.0F, 0.0F);
        bool _is_shoot_requested = false;

        size_t _spawned_projectile_count = 0U;
        std::shared_ptr<Mesh> _projectile_mesh = std::make_shared<Mesh>(sphere);
        std::vector<Entity> _active_projectiles = {};
        std::vector<double> _active_projectile_lifetimes = {};
    };
}
