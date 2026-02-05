#include "runtime.h"
#include "tbx/graphics/renderer.h"
#include "tbx/math/transform.h"

namespace tbx::examples
{
    void AssetExampleRuntimePlugin::on_attach(IPluginHost& context)
    {
        _entity_manager = &context.get_entity_registry();

        auto entity = _entity_manager->create("Green Cube");

        auto& transform = entity.add_component<Transform>();
        transform.position = Vec3(0.0f, 0.0f, -5.0f);
        entity.add_component<Renderer>("Green_Cube.fbx");
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
