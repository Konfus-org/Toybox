#include "tbx/assets/material_assets.h"
#include "tbx/assets/builtin_assets.h"
#include "tbx/assets/messages.h"
#include "tbx/debugging/macros.h"
#include <memory>

namespace tbx
{
    static std::shared_ptr<Material> create_material_data()
    {
        auto material = Material();
        material.program.vertex = unlit_vertex_shader;
        material.program.fragment = unlit_fragment_shader;
        material.textures.set("diffuse", not_found_texture);
        material.parameters.set("color", RgbaColor(1.0f, 0.0f, 1.0f, 1.0f));
        return std::make_shared<Material>(std::move(material));
    }

    AssetPromise<Material> load_material_async(const std::filesystem::path& asset_path)
    {
        auto asset = create_material_data();
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            AssetPromise<Material> result = {};
            result.asset = asset;
            warn_missing_dispatcher("load a material asynchronously");
            result.promise = make_missing_dispatcher_future("load a material asynchronously");
            return result;
        }

        LoadMaterialRequest message(asset_path, asset.get());
        message.not_handled_behavior = MessageNotHandledBehavior::WARN;
        auto future = dispatcher->post(message);
        AssetPromise<Material> result = {};
        result.asset = asset;
        result.promise = future;
        return result;
    }

    std::shared_ptr<Material> load_material(const std::filesystem::path& asset_path)
    {
        auto asset = create_material_data();
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            warn_missing_dispatcher("load a material synchronously");
            return asset;
        }

        LoadMaterialRequest message(asset_path, asset.get());
        message.not_handled_behavior = MessageNotHandledBehavior::WARN;
        auto result = dispatcher->send(message);
        if (!result.succeeded())
        {
            TBX_TRACE_WARNING(
                "Material load request failed for '{}': {}. Using fallback pink material.",
                asset_path.string(),
                result.get_report());
        }
        return asset;
    }
}
