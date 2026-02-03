#include "runtime.h"
#include "tbx/graphics/renderer.h"
#include "tbx/math/transform.h"

namespace tbx::examples
{
    void AssetLoadAndUseExampleRuntimePlugin::on_attach(IPluginHost& context)
    {
        _entity_manager = &context.get_entity_registry();

        auto entity = _entity_manager->create("Green Cube");

        auto& transform = entity.add_component<Transform>();
        transform.position = Vec3(0.0f, 0.0f, -5.0f);
        entity.add_component<Renderer>("Green_Cube.fbx");
    }

    void AssetLoadAndUseExampleRuntimePlugin::on_detach()
    {
        _entity_manager = nullptr;
    }

    void AssetLoadAndUseExampleRuntimePlugin::on_update(const DeltaTime&)
    {
        auto renderers = _entity_manager->get_with<Transform, Renderer>();
        for (auto& entity : renderers)
        {
            auto& transform = entity.get_component<Transform>();
            transform.rotation = Quat({0.0f, 0.01f, 0.0f}) * transform.rotation;
        }
    }

    void AssetLoadAndUseExampleRuntimePlugin::on_recieve_message(Message&) {}
}
