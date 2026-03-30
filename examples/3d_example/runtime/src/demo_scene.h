#pragma once
#include "camera_controller.h"
#include "demo_room.h"
#include "projectile_system.h"
#include "tbx/common/typedefs.h"
#include "tbx/ecs/entity_registry.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/material.h"
#include "tbx/input/input_manager.h"
#include "tbx/physics/collider.h"
#include "tbx/time/delta_time.h"

namespace three_d_example
{
    class DemoScene final
    {
      public:
        DemoScene(tbx::EntityRegistry& entity_registry, tbx::InputManager& input_manager);
        ~DemoScene();

        DemoScene(const DemoScene&) = delete;
        DemoScene(DemoScene&&) = delete;
        DemoScene& operator=(const DemoScene&) = delete;
        DemoScene& operator=(DemoScene&&) = delete;

        void update(const tbx::DeltaTime& dt);

      private:
        void handle_overlap_begin(const tbx::ColliderOverlapEvent& overlap_event);
        void handle_overlap_end(const tbx::ColliderOverlapEvent& overlap_event);
        void log_overlap_event(
            const char* event_name,
            const tbx::ColliderOverlapEvent& overlap_event) const;
        tbx::MaterialInstance create_trigger_zone_material(const tbx::Color& color) const;
        tbx::MaterialInstance create_falling_box_material() const;
        void set_trigger_zone_color(const tbx::Color& color);

      private:
        tbx::EntityRegistry* _entity_registry = nullptr;
        DemoRoom _demo_room;
        ProjectileSystem _projectile_system;
        CameraController _camera_controller;
        tbx::Entity _sun = {};
        tbx::Entity _area_light = {};
        tbx::Entity _trigger_zone = {};
        tbx::Entity _falling_sphere = {};
        tbx::Entity _falling_box = {};
        size _trigger_overlap_count = 0U;
    };
}
