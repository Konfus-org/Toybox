#include "demo_scene.h"
#include "tbx/assets/builtin_assets.h"
#include "tbx/debugging/macros.h"
#include "tbx/ecs/entity.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/light.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/mesh.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"
#include "tbx/physics/collider.h"
#include "tbx/physics/physics.h"

namespace three_d_example
{
    DemoScene::DemoScene(tbx::EntityRegistry& entity_registry, tbx::IInputManager& input_manager)
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
                  .move_speed = 10.0F,
                  .look_sensitivity = 0.0035F,
              })
    {
        _sun = tbx::Entity("Sun", entity_registry);
        _sun.add_component<tbx::DirectionalLight>(tbx::Color::WHITE, 1.0F, 0.15F);
        _sun.add_component<tbx::Transform>(
            tbx::Vec3(0.0F, 0.0F, 0.0F),
            tbx::Quat(tbx::to_radians(tbx::Vec3(-45.0F, 45.0F, 0.0F))),
            tbx::Vec3(1.0F, 1.0F, 1.0F));

        _area_light = tbx::Entity("AreaLight", entity_registry);
        _area_light.add_component(tbx::AreaLight());
        _area_light.add_component<tbx::Transform>(
            tbx::Vec3(0.0F, 5.0F, 0.0F),
            tbx::Quat(tbx::to_radians(tbx::Vec3(-180.0F, 0.0F, 0.0F))),
            tbx::Vec3(1.0F, 1.0F, 1.0F));

        _sky = tbx::Entity("Sky", entity_registry);
        _sky.add_component<tbx::Sky>(
            tbx::MaterialInstance(tbx::Handle("Materials/AnimeSkybox.mat")));

        _post_processing = tbx::Entity("PostProcessing", entity_registry);
        auto lut_post_process_material =
            tbx::MaterialInstance(tbx::Handle("Materials/LutPostProcess.mat"));
        auto lut_effect = tbx::PostProcessingEffect {};
        lut_effect.material = lut_post_process_material;
        lut_effect.blend = 1.0F;
        _post_processing.add_component<tbx::PostProcessing>(tbx::PostProcessing {
            .effects = {lut_effect},
            .is_enabled = true,
        });

        _trigger_zone = tbx::Entity("TriggerZone", entity_registry);
        _trigger_zone.add_component<tbx::MaterialInstance>(
            create_trigger_zone_material(tbx::Color::RED));
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
        auto falling_sphere_material = tbx::MaterialInstance(tbx::PbrMaterial::HANDLE);
        falling_sphere_material.set_parameter(tbx::PbrMaterial::COLOR, tbx::Color::RED);
        _falling_sphere.add_component<tbx::MaterialInstance>(falling_sphere_material);
        _falling_sphere.add_component<tbx::DynamicMesh>(tbx::sphere);
        _falling_sphere.add_component<tbx::Transform>(tbx::Vec3(0.0F, 6.0F, -5.2F));
        _falling_sphere.add_component<tbx::SphereCollider>(0.5F);
        _falling_sphere.add_component<tbx::Physics>();

        _falling_box = tbx::Entity("FallingBox", entity_registry);
        _falling_box.add_component<tbx::MaterialInstance>(create_falling_box_material());
        _falling_box.add_component<tbx::StaticMesh>(tbx::Handle("Models/Green_Cube.fbx"));
        _falling_box.add_component<tbx::Transform>(tbx::Vec3(0.0F, 10.0F, -4.9F));
        _falling_box.add_component<tbx::MeshCollider>();
        _falling_box.add_component<tbx::Physics>();
    }

    DemoScene::~DemoScene()
    {
        if (_trigger_zone.get_id().is_valid() && _trigger_zone.has_component<tbx::CubeCollider>())
        {
            auto& collider = _trigger_zone.get_component<tbx::CubeCollider>();
            collider.trigger.is_overlap_enabled = false;
            collider.trigger.overlap_begin_callbacks.clear();
            collider.trigger.overlap_stay_callbacks.clear();
            collider.trigger.overlap_end_callbacks.clear();
        }

        _sun.destroy();
        _sun = {};
        _area_light.destroy();
        _area_light = {};
        _sky.destroy();
        _sky = {};
        _post_processing.destroy();
        _post_processing = {};
        _trigger_zone.destroy();
        _trigger_zone = {};
        _falling_box.destroy();
        _falling_box = {};
        _falling_sphere.destroy();
        _falling_sphere = {};
        _trigger_overlap_count = 0U;
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
        auto material = tbx::MaterialInstance(tbx::WireframeMaterial::HANDLE);
        material.set_parameter(tbx::WireframeMaterial::COLOR, color);
        material.set_parameter(tbx::WireframeMaterial::WIREFRAME_WIDTH, 1.0F);
        return material;
    }

    tbx::MaterialInstance DemoScene::create_falling_box_material() const
    {
        auto material = tbx::MaterialInstance(tbx::PbrMaterial::HANDLE);
        material.set_parameter(tbx::PbrMaterial::COLOR, tbx::Color::GREEN);
        material.set_parameter(tbx::PbrMaterial::COLOR_TEXTURE_BLEND, 0.45F);
        material.set_parameter(tbx::PbrMaterial::ALPHA_CUTOFF, 0.0F);
        material.set_texture(tbx::PbrMaterial::DIFFUSE_MAP, tbx::Handle("Textures/Smily.png"));
        return material;
    }

    void DemoScene::set_trigger_zone_color(const tbx::Color& color)
    {
        if (!_trigger_zone.get_id().is_valid()
            || !_trigger_zone.has_component<tbx::MaterialInstance>())
            return;

        _trigger_zone.get_component<tbx::MaterialInstance>() = create_trigger_zone_material(color);
    }
}
