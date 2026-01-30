#include "runtime.h"
#include "tbx/app/application.h"
#include "tbx/assets/texture_assets.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/model.h"
#include "tbx/math/transform.h"

namespace tbx::examples
{
    void AssetLoadAndUseExampleRuntimePlugin::on_attach(IPluginHost& context)
    {
        _entity_manager = &context.get_entity_manager();

        auto model = context.get_asset_manager().load<Model>({"Green_Cube.fbx"});
        auto entity = _entity_manager->create_entity("Green Cube");

        auto transform = entity.add_component<Transform>();
        transform.scale = Vec3(0.1f, 0.1f, 0.1f);
        transform.position = Vec3(0.0f, 0.0f, 125.0f);
        entity.add_component<Model>(*model.get());
    }

    void AssetLoadAndUseExampleRuntimePlugin::on_detach()
    {
        _entity_manager = nullptr;
    }

    void AssetLoadAndUseExampleRuntimePlugin::on_update(const DeltaTime&)
    {
        auto models = _entity_manager->get_entities_with<Transform, Model>();
        for (auto& entity : models)
        {
            auto& transform = entity.get_component<Transform>();
            transform.rotation = Quat({0.0f, 0.01f, 0.0f}) * transform.rotation;
            transform.scale = Vec3(0.1f, 0.1f, 0.1f);
            transform.position = Vec3(0.0f, 0.0f, -125.0f);
        }
    }

    void AssetLoadAndUseExampleRuntimePlugin::on_recieve_message(Message&) {}
}
