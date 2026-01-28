#include "runtime.h"
#include "tbx/app/application.h"
#include "tbx/assets/texture_assets.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/model.h"
#include "tbx/math/transform.h"

namespace tbx::examples
{
    void AssetLoadAndUseExampleRuntimePlugin::on_attach(Application& context)
    {
        _ecs = &context.get_ecs();
        TBX_TRACE_INFO("AssetLoadAndUseExample: loading Smily texture.");

        auto texture_asset = load_texture(
            "Smily.png",
            TextureWrap::Repeat,
            TextureFilter::Nearest,
            TextureFormat::RGBA);
        _smily_texture = texture_asset;

        Model quad_model = {};
        quad_model.mesh = quad;
        quad_model.material = Material();
        quad_model.material.textures = {_smily_texture};

        auto entity = _ecs->create_entity("SmilyQuad");
        entity.add_component<Transform>();
        entity.add_component<Model>(quad_model);
    }

    void AssetLoadAndUseExampleRuntimePlugin::on_detach()
    {
        _smily_texture.reset();
        _ecs = nullptr;
    }

    void AssetLoadAndUseExampleRuntimePlugin::on_update(const DeltaTime&) {}

    void AssetLoadAndUseExampleRuntimePlugin::on_recieve_message(Message&) {}
}
