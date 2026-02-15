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
    void AssetExampleRuntimePlugin::on_attach(IPluginHost& context)
    {
        _ent_registry = &context.get_entity_registry();

        // Setup assets to use
        auto smily_mat = Handle("Materials/Smily.mat");
        auto skybox_mat = Handle("Materials/AnimeSkybox.mat");
        auto lut_post_mat = Handle("Materials/LutPostProcess.mat");
        auto skybox_tex = Handle("Textures/AnimeSkybox.png");
        auto lut_tex = Handle("Textures/LUTs/LUT_Cinematic.png");
        auto green_cube = Handle("Models/Green_Cube.fbx");

        // Setup light
        _sun = _ent_registry->create("Light");
        _sun.add_component<DirectionalLight>(RgbaColor::yellow, 1, 0.25f);
        _sun.add_component<Transform>(Vec3(0), to_radians(Vec3(-45.0f, 45.0f, 0.0f)), Vec3(1));

        // Setup cube
        _green_cube = _ent_registry->create("Cube");
        _green_cube.add_component<Renderer>();
        _green_cube.add_component<StaticMesh>(green_cube);
        _green_cube.add_component<Transform>(Vec3(0.0f, 0.0f, -5.0f));

        // Setup ground
        auto ground_ent = _ent_registry->create("Ground");
        ground_ent.add_component<Renderer>(smily_mat);
        ground_ent.add_component<DynamicMesh>(quad);
        ground_ent.add_component<Transform>(
            Vec3(0.0f, -2.0f, -5.0f),
            to_radians(Vec3(-90.0f, 0.0f, 0.0f)),
            Vec3(20.0f, 20.0f, 1.0f));

        // Setup sky
        auto sky_ent = _ent_registry->create("Sky");
        sky_ent.add_component<Sky>(skybox_mat);

        // Setup global post-processing stack
        auto post_ent = _ent_registry->create("PostProcessing");
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
        auto cam_ent = _ent_registry->create("Camera");
        cam_ent.add_component<Camera>();
        cam_ent.add_component<Transform>(Vec3(0.0f, 0.0f, 10.0f));
    }

    void AssetExampleRuntimePlugin::on_detach()
    {
        _ent_registry = nullptr;
    }

    void AssetExampleRuntimePlugin::on_update(const DeltaTime& dt)
    {
        // Rotate cube...
        {
            auto& transform = _green_cube.get_component<Transform>();
            const float YAW_RATE = 2;
            double angle = YAW_RATE * dt.seconds;
            auto delta = Quat({0.0f, angle, 0.0f});
            transform.rotation = normalize(delta * transform.rotation);
        }

        // Rotate light...
        {
            auto& transform = _sun.get_component<Transform>();
            const float YAW_RATE = 0.5f;
            double angle = YAW_RATE * dt.seconds;
            auto delta = Quat({0.0f, angle, 0.0f});
            transform.rotation = normalize(delta * transform.rotation);
        }
    }
}
