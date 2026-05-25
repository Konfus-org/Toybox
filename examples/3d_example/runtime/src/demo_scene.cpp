#include "demo_scene.h"
#include "tbx/systems/assets/builtin_assets.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/types/color.h"
#include "tbx/types/components/collider.h"
#include "tbx/types/components/light.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/post_processing.h"
#include "tbx/types/components/rigidbody.h"
#include "tbx/types/components/sky.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/material.h"
#include "tbx/types/trig.h"

namespace three_d_example
{
    DemoScene::DemoScene(
        std::weak_ptr<tbx::EntityRegistry> entity_registry,
        std::weak_ptr<tbx::IInputManager> input_manager,
        std::weak_ptr<tbx::Physics> physics)
        : _entity_registry(entity_registry)
    {
        auto entity_registry_lock = _entity_registry.lock();
        if (!entity_registry_lock)
            return;

        _demo_room = std::make_unique<DemoRoom>(
            *entity_registry_lock,
            DemoRoomSettings {
                .center = tbx::Vec3(0.0F, 0.0F, 0.0F),
                .include_colliders = true,
            });
        _projectile_system = std::make_shared<ProjectileSystem>(
            _entity_registry,
            [this]()
            {
                if (!_camera_controller)
                    return tbx::Entity {};

                return _camera_controller->get_camera();
            });
        _camera_controller = std::make_unique<CameraController>(
            _entity_registry,
            input_manager,
            physics,
            _projectile_system,
            CameraControllerSettings {
                .initial_position = tbx::Vec3(0.0F, 2.01F, 11.0F),
                .initial_yaw = 0.0F,
                .initial_pitch = tbx::to_radians(-8.0F),
                .move_speed = 10.0F,
                .look_sensitivity = 0.0035F,
            });

        auto& registry = *entity_registry_lock;
        _sun = tbx::Entity("Sun", registry);
        _sun.add_component<tbx::DirectionalLight>(tbx::Color::WHITE, 1.0F, 0.15F);
        _sun.add_component<tbx::Transform>(
            tbx::Vec3(0.0F, 0.0F, 0.0F),
            tbx::Quat(tbx::to_radians(tbx::Vec3(-45.0F, 45.0F, 0.0F))),
            tbx::Vec3(1.0F, 1.0F, 1.0F));

        _area_light = tbx::Entity("AreaLight", registry);
        _area_light.add_component<tbx::AreaLight>();
        _area_light.add_component<tbx::Transform>(
            tbx::Vec3(0.0F, 5.0F, 0.0F),
            tbx::Quat(tbx::to_radians(tbx::Vec3(-180.0F, 0.0F, 0.0F))),
            tbx::Vec3(1.0F, 1.0F, 1.0F));

        _sky = tbx::Entity("Sky", registry);
        _sky.add_component<tbx::Sky>(tbx::MaterialInstance(
            tbx::TexturedSkyMaterial::HANDLE,
            tbx::MaterialParameterBindings {
                {tbx::TexturedSkyMaterial::COLOR, tbx::Color::WHITE},
                {tbx::TexturedSkyMaterial::BRIGHTNESS, 1.0F},
            },
            tbx::MaterialTextureBindings {
                tbx::MaterialTextureBinding(
                    tbx::make_param_id(tbx::TexturedSkyMaterial::SKYBOX_TEXTURE),
                    tbx::Handle("Textures/AnimeSkybox.png")),
            }));
        _sky.add_component<tbx::Transform>(
            tbx::Vec3(0.0F, 0.0F, 0.0F),
            tbx::Quat(tbx::to_radians(tbx::Vec3(0.0F, 180.0F, 0.0F))),
            tbx::Vec3(1.0F, 1.0F, 1.0F));
        _sky_system.set_sky_entity(_sky);

        _post_processing = tbx::Entity("PostProcessing", registry);
        auto lut_post_process_material =
            tbx::MaterialInstance(tbx::Handle("Materials/LutPostProcess.mat"));
        auto lut_effect = tbx::PostProcessingEffect {};
        lut_effect.material = lut_post_process_material;
        lut_effect.blend = 1.0F;
        auto post_processing = tbx::PostProcessing {};
        post_processing.effects = {lut_effect};
        post_processing.is_enabled = true;
        _post_processing.add_component<tbx::PostProcessing>(post_processing);

        _trigger_zone = tbx::Entity("TriggerZone", registry);
        _trigger_zone.add_component<tbx::MaterialInstance>(
            create_trigger_zone_material(tbx::Color::RED));
        _trigger_zone.add_component<tbx::DynamicMesh>(tbx::Mesh::CUBE);
        _trigger_zone.add_component<tbx::Transform>(
            tbx::Vec3(0.0F, 2.5F, -5.0F),
            tbx::Quat(tbx::Vec3(0.0F, 0.0F, 0.0F)),
            tbx::Vec3(3.0F, 3.0F, 3.0F));
        _trigger_zone.add_component<tbx::CubeCollider>(
            tbx::Vec3(1.5F, 1.5F, 1.5F),
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
            });

        _falling_sphere = tbx::Entity("FallingSphere", registry);
        auto falling_sphere_material = tbx::MaterialInstance(tbx::PbrMaterial::HANDLE);
        falling_sphere_material.set_parameter(tbx::PbrMaterial::ALBEDO_COLOR, tbx::Color::RED);
        _falling_sphere.add_component<tbx::MaterialInstance>(falling_sphere_material);
        _falling_sphere.add_component<tbx::DynamicMesh>(tbx::Mesh::SPHERE);
        _falling_sphere.add_component<tbx::Transform>(tbx::Vec3(0.0F, 6.0F, -5.2F));
        _falling_sphere.add_component<tbx::SphereCollider>(0.5F);
        _falling_sphere.add_component<tbx::Rigidbody>();

        _falling_box = tbx::Entity("FallingBox", registry);
        _falling_box.add_component<tbx::MaterialInstance>(create_falling_box_material());
        _falling_box.add_component<tbx::StaticMesh>(tbx::Handle("Models/Green_Cube.fbx"));
        _falling_box.add_component<tbx::Transform>(tbx::Vec3(0.0F, 10.0F, -4.9F));
        _falling_box.add_component<tbx::MeshCollider>();
        _falling_box.add_component<tbx::Rigidbody>();
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
        _camera_controller.reset();
        _projectile_system.reset();
        _demo_room.reset();
        _entity_registry.reset();
    }

    void DemoScene::update(const tbx::DeltaTime& dt)
    {
        if (_camera_controller)
            _camera_controller->update(dt);
        if (_projectile_system)
            _projectile_system->update(dt);
        _sky_system.update(dt);
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
        auto entity_registry = _entity_registry.lock();
        if (!entity_registry)
            return;

        TBX_TRACE_INFO(
            "Trigger overlap {}: {} with {}.",
            event_name,
            entity_registry->get(overlap_event.trigger_entity_id).get_name(),
            entity_registry->get(overlap_event.overlapped_entity_id).get_name());
    }

    tbx::MaterialInstance DemoScene::create_trigger_zone_material(const tbx::Color& color) const
    {
        constexpr float trigger_volume_alpha = 0.35F;

        auto material = tbx::MaterialInstance(tbx::TriggerVolumeMaterial::HANDLE);
        material.set_parameter(
            tbx::TriggerVolumeMaterial::ALBEDO_COLOR,
            tbx::Color(color.r, color.g, color.b, trigger_volume_alpha));
        return material;
    }

    tbx::MaterialInstance DemoScene::create_falling_box_material() const
    {
        auto material = tbx::MaterialInstance(tbx::PbrMaterial::HANDLE);
        material.set_parameter(tbx::PbrMaterial::ALBEDO_COLOR, tbx::Color::GREEN);
        material.set_texture(tbx::PbrMaterial::ALBEDO_MAP, tbx::Handle("Textures/Smily.png"));
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
