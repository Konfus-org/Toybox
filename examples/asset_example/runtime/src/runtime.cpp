#include "runtime.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/light.h"
#include "tbx/graphics/renderer.h"
#include "tbx/math/transform.h"

namespace tbx::examples
{
    void AssetExampleRuntimePlugin::on_attach(IPluginHost& context)
    {
        _entity_manager = &context.get_entity_registry();

        // Setup smily cube
        auto smily_cube_ent = _entity_manager->create("Smily Cube");
        auto& smily_ren =
            smily_cube_ent.add_component<Renderer>("Green_Cube.fbx", Handle("Smily.mat"));
        auto& smily_tran = smily_cube_ent.add_component<Transform>();
        smily_tran.position = Vec3(0.0f, 0.0f, -5.0f);

        // Setup ground
        auto ground_ent = _entity_manager->create("Ground");
        auto& ground_ren = ground_ent.add_component<Renderer>("Green_Cube.fbx");
        auto& ground_tran = ground_ent.add_component<Transform>();
        ground_tran.scale = Vec3(10);
        ground_tran.position = Vec3(0.0f, -2.0f, -5.0f);
        ground_tran.scale = Vec3(10.0f, 0.1f, 10.0f);

        // Setup light
        auto light_ent = _entity_manager->create("Light");
        auto& light = light_ent.add_component<Light>();
        light.color = RgbaColor::white;
    }

    void AssetExampleRuntimePlugin::on_detach()
    {
        _entity_manager = nullptr;
    }

    void AssetExampleRuntimePlugin::on_update(const DeltaTime& dt)
    {
        auto renderers = _entity_manager->get_with<Transform, Renderer>();
        for (auto& entity : renderers)
        {
            auto desc = entity.get_component<EntityDescription>();
            if (desc.name == "Ground")
                continue;

            auto& transform = entity.get_component<Transform>();

            // angular speed in radians per second around Y
            const float YAW_RATE = 2;
            double angle = YAW_RATE * dt.seconds;

            auto delta = Quat({0.0f, angle, 0.0f});
            transform.rotation = normalize(delta * transform.rotation);
        }
    }

    void AssetExampleRuntimePlugin::on_recieve_message(Message&) {}
}
