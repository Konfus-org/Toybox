#include "runtime.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/light.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/renderer.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"

namespace tbx::examples
{
    void AssetExampleRuntimePlugin::on_attach(IPluginHost& host)
    {
        auto& ent_registry = host.get_entity_registry();
        auto& input_manager = host.get_input_manager();

        // Setup assets to use
        auto skybox_mat = Handle("Materials/AnimeSkybox.mat");
        auto lut_post_mat = Handle("Materials/LutPostProcess.mat");
        auto lut_tex = Handle("Textures/LUTs/LUT_Cinematic.png");
        auto green_cube = Handle("Models/Green_Cube.fbx");

        // Setup light
        _sun = Entity("Light", ent_registry);
        _sun.add_component<DirectionalLight>(Color::YELLOW, 1.0f, 0.25f);
        _sun.add_component<Transform>(Vec3(0), to_radians(Vec3(-45.0f, 45.0f, 0.0f)), Vec3(1));

        // Setup cube
        _cube = Entity("Cube", ent_registry);
        _cube.add_component<Renderer>();
        _cube.add_component<StaticMesh>(green_cube);
        _cube.add_component<Transform>(Vec3(0.0f, 0.0f, -5.0f));

        _room.create(
            ent_registry,
            RoomSettings {
                .center = Vec3(0.0F, -2.0F, -5.0F),
                .include_colliders = true,
            });

        // Setup sky
        auto sky_ent = Entity("Sky", ent_registry);
        sky_ent.add_component<Sky>(skybox_mat);

        // Setup global post-processing stack
        auto post_ent = Entity("PostProcessing", ent_registry);
        auto post_processing = PostProcessing({
            PostProcessingEffect {
                .material =
                    {
                        .handle = lut_post_mat,
                        .textures = {{
                            .name = "lut",
                            .texture = {lut_tex},
                        }},
                    },
                .blend = 1.0f,
            },
        });
        post_ent.add_component<PostProcessing>(post_processing);

        // Setup camera
        auto camera = Entity("Camera", ent_registry);
        camera.add_component<Camera>();
        camera.add_component<Transform>(Vec3(0.0F, 0.0F, 10.0F));

        _camera_controller.initialize(
            camera,
            input_manager,
            CameraControllerSettings {
                .initial_yaw = 0.0F,
                .initial_pitch = 0.0F,
                .move_speed = 6.0F,
                .look_sensitivity = 0.0025F,
            });
    }

    void AssetExampleRuntimePlugin::on_detach()
    {
        _camera_controller.shutdown(get_host().get_input_manager());
        _room.destroy();
    }

    void AssetExampleRuntimePlugin::on_update(const DeltaTime& dt)
    {
        // Rotate cube...
        {
            auto& transform = _cube.get_component<Transform>();
            const float yaw_rate = 1.0F;
            float angle = yaw_rate * static_cast<float>(dt.seconds);
            auto delta = Quat({0.0F, angle, 0.0F});
            transform.rotation = normalize(delta * transform.rotation);
        }

        // Rotate light...
        {
            auto& transform = _sun.get_component<Transform>();
            const float yaw_rate = 0.5F;
            float angle = yaw_rate * static_cast<float>(dt.seconds);
            auto delta = Quat({0.0F, angle, 0.0F});
            transform.rotation = normalize(delta * transform.rotation);
        }

        _camera_controller.update(dt);
    }
}
