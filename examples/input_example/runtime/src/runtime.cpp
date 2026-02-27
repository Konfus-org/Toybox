#include "runtime.h"
#include "tbx/app/app_settings.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/light.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/renderer.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"

namespace input_example
{
    void InputExampleRuntimePlugin::on_attach(tbx::IPluginHost& host)
    {
        auto& ent_registry = host.get_entity_registry();
        auto& input_manager = host.get_input_manager();
        auto& graphics = host.get_settings().graphics;
        graphics.shadow_map_resolution = 2048U;
        graphics.shadow_render_distance = 40.0F;
        graphics.shadow_softness = 1.1F;

        _sun = tbx::Entity("Light", ent_registry);
        _sun.add_component<DirectionalLight>(Color::WHITE, 1.0F, 0.25F);
        _sun.add_component<tbx::Transform>(tbx::Vec3(0), tbx::to_radians(tbx::Vec3(-45.0F, 45.0F, 0.0F)), tbx::Vec3(1));

        auto ground_ent = tbx::Entity("Ground", ent_registry);
        ground_ent.add_component<Renderer>();
        ground_ent.add_component<DynamicMesh>(quad);
        ground_ent.add_component<tbx::Transform>(
            tbx::Vec3(0.0F, 0.0F, 0.0F),
            tbx::to_radians(tbx::Vec3(-90.0F, 0.0F, 0.0F)),
            tbx::Vec3(20.0F, 20.0F, 1.0F));

        auto cube_ent = tbx::Entity("Cube", ent_registry);
        cube_ent.add_component<Renderer>(MaterialInstance {
            .parameters = {{"color", Color::YELLOW}},
        });
        cube_ent.add_component<DynamicMesh>(cube);
        cube_ent.add_component<tbx::Transform>(tbx::Vec3(0.0F, 0.5F, -5.0F));

        _player_controller.initialize(
            ent_registry,
            input_manager,
            "InputExample.Camera",
            examples_common::PlayerControllerSettings {
                .player_spawn_position = tbx::Vec3(0.0F, 0.01F, 0.0F),
                .visual_local_position = tbx::Vec3(0.0F, 1.0F, 0.0F),
                .visual_local_scale = tbx::Vec3(1.5F, 2.0F, 1.5F),
                .camera_local_position = tbx::Vec3(0.0F, 2.0F, -0.55F),
                .initial_yaw = 0.0F,
                .initial_pitch = 0.0F,
                .move_speed = 6.0F,
                .look_sensitivity = 0.0025F,
            });
    }

    void InputExampleRuntimePlugin::on_detach()
    {
        _player_controller.shutdown(get_host().get_input_manager());
    }

    void InputExampleRuntimePlugin::on_update(const tbx::DeltaTime& dt)
    {
        _player_controller.update(dt);
    }
}
