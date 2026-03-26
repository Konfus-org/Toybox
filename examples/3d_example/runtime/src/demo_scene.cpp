#include "demo_scene.h"
#include "builtin_assets.generated.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/light.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/renderer.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"
#include "tbx/physics/collider.h"
#include "tbx/physics/physics.h"

namespace three_d_example
{
    DemoScene::DemoScene(
        tbx::EntityRegistry& entity_registry,
        tbx::InputManager& input_manager)
        : _entity_registry(&entity_registry)
        , _demo_room(
              entity_registry,
              DemoRoomSettings {
                  .center = tbx::Vec3(0.0F, 0.0F, 0.0F),
                  .include_colliders = true,
              })
        , _projectile_system(
              entity_registry,
              [this]()
              {
                  return _camera_controller.get_camera();
              })
        , _camera_controller(
              entity_registry,
              input_manager,
              _projectile_system,
              CameraControllerSettings {
                  .initial_position = tbx::Vec3(0.0F, 2.01F, 11.0F),
                  .initial_yaw = 0.0F,
                  .initial_pitch = tbx::to_radians(-8.0F),
                  .move_speed = 6.0F,
                  .look_sensitivity = 0.0025F,
              })
    {
        _sun = tbx::Entity("Sun", entity_registry);
        _sun.add_component<tbx::DirectionalLight>(tbx::Color::WHITE, 1.0F, 0.15F);
        _sun.add_component<tbx::Transform>(
            tbx::Vec3(0.0F, 0.0F, 0.0F),
            tbx::Quat(tbx::to_radians(tbx::Vec3(-45.0F, 45.0F, 0.0F))),
            tbx::Vec3(1.0F, 1.0F, 1.0F));

        _trigger_zone = tbx::Entity("TriggerZone", entity_registry);
        auto trigger_zone_renderer = tbx::Renderer {};
        trigger_zone_renderer.material = create_trigger_zone_material(tbx::Color::RED);
        trigger_zone_renderer.is_two_sided = true;
        trigger_zone_renderer.shadow_mode = tbx::ShadowMode::None;
        _trigger_zone.add_component<tbx::Renderer>(trigger_zone_renderer);
        _trigger_zone.add_component<tbx::DynamicMesh>(tbx::cube);
        _trigger_zone.add_component<tbx::Transform>(
            tbx::Vec3(0.0F, 2.5F, -5.0F),
            tbx::Quat(tbx::Vec3(0.0F, 0.0F, 0.0F)),
            tbx::Vec3(3.0F, 3.0F, 3.0F));
        _trigger_zone.add_component<tbx::CubeCollider>(tbx::CubeCollider {
            .half_extents = tbx::Vec3(1.5F, 1.5F, 1.5F),
            .trigger =
                tbx::ColliderTrigger {
                    .is_trigger_only = true,
                    .is_overlap_enabled = true,
                    .overlap_begin_callbacks =
                        {
                            [this](const tbx::ColliderOverlapEvent& overlap_event)
                            {
                                handle_overlap_begin(overlap_event);
                            },
                        },
                    .overlap_end_callbacks =
                        {
                            [this](const tbx::ColliderOverlapEvent& overlap_event)
                            {
                                handle_overlap_end(overlap_event);
                            },
                        },
                },
        });

        _falling_sphere = tbx::Entity("FallingSphere", entity_registry);
        _falling_sphere.add_component<tbx::Renderer>(tbx::PbrMaterialInstance(tbx::Color::RED));
        _falling_sphere.add_component<tbx::DynamicMesh>(tbx::sphere);
        _falling_sphere.add_component<tbx::Transform>(tbx::Vec3(0.0F, 6.0F, -5.2F));
        _falling_sphere.add_component<tbx::SphereCollider>(0.5F);
        _falling_sphere.add_component<tbx::Physics>();

        _falling_box = tbx::Entity("FallingBox", entity_registry);
        _falling_box.add_component<tbx::Renderer>(tbx::PbrMaterialInstance(tbx::Color::GREEN));
        _falling_box.add_component<tbx::DynamicMesh>(tbx::cube);
        _falling_box.add_component<tbx::Transform>(tbx::Vec3(0.0F, 10.0F, -4.9F));
        _falling_box.add_component<tbx::CubeCollider>(tbx::Vec3(0.5F, 0.5F, 0.5F));
        _falling_box.add_component<tbx::Physics>();
    }

    DemoScene::~DemoScene()
    {
        _sun.destroy();
        _sun = {};
        _trigger_zone.destroy();
        _trigger_zone = {};
        _falling_box.destroy();
        _falling_box = {};
        _falling_sphere.destroy();
        _falling_sphere = {};
        _entity_registry = nullptr;
    }

    void DemoScene::update(const tbx::DeltaTime& dt)
    {
        _camera_controller.update(dt);
        _projectile_system.update(dt);
    }

    void DemoScene::handle_overlap_begin(const tbx::ColliderOverlapEvent& overlap_event)
    {
        _trigger_overlap_count += 1U;
        set_trigger_zone_color(tbx::Color::GREEN);
        log_overlap_event("begin", overlap_event);
    }

    void DemoScene::handle_overlap_end(const tbx::ColliderOverlapEvent& overlap_event)
    {
        if (_trigger_overlap_count > 0U)
            _trigger_overlap_count -= 1U;

        if (_trigger_overlap_count == 0U)
            set_trigger_zone_color(tbx::Color::RED);

        log_overlap_event("end", overlap_event);
    }

    void DemoScene::log_overlap_event(
        const char* event_name,
        const tbx::ColliderOverlapEvent& overlap_event) const
    {
        if (_entity_registry == nullptr)
            return;

        TBX_TRACE_INFO(
            "Trigger overlap {}: {} with {}.",
            event_name,
            _entity_registry->get(overlap_event.trigger_entity_id).get_name(),
            _entity_registry->get(overlap_event.overlapped_entity_id).get_name());
    }

    tbx::MaterialInstance DemoScene::create_trigger_zone_material(const tbx::Color& color) const
    {
        auto material = tbx::FlatMaterialInstance(
            color,
            tbx::Color::BLACK,
            0.1F,
            tbx::Handle(),
            tbx::wireframe_material);
        material.parameters.set("wireframe_width", 1.6F);
        return material;
    }

    void DemoScene::set_trigger_zone_color(const tbx::Color& color)
    {
        if (!_trigger_zone.get_id().is_valid() || !_trigger_zone.has_component<tbx::Renderer>())
            return;

        auto& renderer = _trigger_zone.get_component<tbx::Renderer>();
        renderer.material = create_trigger_zone_material(color);
    }
}
