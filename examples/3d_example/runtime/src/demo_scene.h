#pragma once
#include "camera_controller.h"
#include "demo_room.h"
#include "projectile_system.h"
#include "sky_system.h"
#include "tbx/interfaces/input_manager.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/physics/physics.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/color.h"
#include "tbx/types/components/collider.h"
#include "tbx/types/material.h"
#include "tbx/types/typedefs.h"
#include <memory>

namespace three_d_example
{
    class DemoScene final
    {
      public:
        DemoScene(
            std::weak_ptr<tbx::EntityRegistry> entity_registry,
            std::weak_ptr<tbx::IInputManager> input_manager,
            std::weak_ptr<tbx::Physics> physics);
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
        std::weak_ptr<tbx::EntityRegistry> _entity_registry = {};

        std::unique_ptr<DemoRoom> _demo_room = {};
        std::shared_ptr<ProjectileSystem> _projectile_system = {};
        std::unique_ptr<CameraController> _camera_controller = {};
        SkySystem _sky_system = {};

        tbx::Entity _sun = {};
        tbx::Entity _area_light = {};
        tbx::Entity _sky = {};
        tbx::Entity _post_processing = {};
        tbx::Entity _trigger_zone = {};
        tbx::Entity _falling_sphere = {};
        tbx::Entity _falling_box = {};

        size _trigger_overlap_count = 0U;
    };
}
