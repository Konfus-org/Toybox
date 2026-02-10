#include "runtime.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/light.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/renderer.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"
#include <memory>

namespace tbx::examples
{
    void AssetExampleRuntimePlugin::on_attach(IPluginHost& context)
    {
        _entity_manager = &context.get_entity_registry();

        // Setup assets to use
        auto smily_mat = Handle("Smily.mat");
        auto green_cube = Handle("Green_Cube.fbx");

        // Setup light
        _sun = _entity_manager->create("Light");
        _sun.add_component<DirectionalLight>(RgbaColor::yellow, 1);
        _sun.add_component<Transform>(Vec3(0), to_radians(Vec3(-45.0f, 45.0f, 0.0f)), Vec3(1));

        // Setup cube
        _green_cube = _entity_manager->create("Cube");
        _green_cube.add_component<Renderer>();
        _green_cube.add_component<StaticMesh>(green_cube);
        _green_cube.add_component<Transform>(Vec3(0.0f, 0.0f, -5.0f));

        // Setup ground
        auto ground_ent = _entity_manager->create("Ground");
        ground_ent.add_component<Renderer>(smily_mat);
        ground_ent.add_component<ProceduralMesh>(quad);
        ground_ent.add_component<Transform>(
            Vec3(0.0f, -2.0f, -5.0f),
            to_radians(Vec3(-90.0f, 0.0f, 0.0f)),
            Vec3(20.0f, 20.0f, 1.0f));

        // Setup camera
        auto cam_ent = _entity_manager->create("Camera");
        cam_ent.add_component<Camera>();
        cam_ent.add_component<Transform>(
            Vec3(0.0f, 5.0f, 10.0f),
            to_radians(Vec3(-25.0f, 0.0f, 0.0f)));
    }

    void AssetExampleRuntimePlugin::on_detach()
    {
        _entity_manager = nullptr;
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
