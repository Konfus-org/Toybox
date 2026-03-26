#pragma once
#include "camera_controller.h"
#include "demo_room.h"
#include "projectile_system.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/ecs/entity_registry.h"
#include "tbx/input/input_manager.h"
#include "tbx/physics/collider.h"
#include "tbx/time/delta_time.h"

namespace three_d_example
{
    class DemoScene final
    {
      public:
        DemoScene(
            tbx::EntityRegistry& entity_registry,
            tbx::InputManager& input_manager,
            tbx::AssetManager& asset_manager);
        ~DemoScene();

        DemoScene(const DemoScene&) = delete;
        DemoScene(DemoScene&&) = delete;
        DemoScene& operator=(const DemoScene&) = delete;
        DemoScene& operator=(DemoScene&&) = delete;

        void update(const tbx::DeltaTime& dt);

      private:
        void log_overlap_event(
            const char* event_name,
            const tbx::ColliderOverlapEvent& overlap_event) const;

      private:
        tbx::EntityRegistry* _entity_registry = nullptr;
        DemoRoom _demo_room;
        ProjectileSystem _projectile_system;
        CameraController _camera_controller;
        tbx::Entity _sun = {};
        tbx::Entity _trigger_zone = {};
        tbx::Entity _falling_sphere = {};
        tbx::Entity _falling_box = {};
    };
}
