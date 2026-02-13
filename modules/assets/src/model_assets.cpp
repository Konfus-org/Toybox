#include "tbx/assets/model_assets.h"
#include "tbx/assets/builtin_assets.h"
#include "tbx/assets/messages.h"
#include "tbx/graphics/model.h"
#include <memory>

namespace tbx
{
    static Mesh create_two_sided_fallback_mesh()
    {
        auto mesh = Mesh(quad.vertices, quad.indices);
        const auto original_index_count = mesh.indices.size();
        mesh.indices.reserve(original_index_count * 2U);
        for (size_t index = 0U; index + 2U < original_index_count; index += 3U)
        {
            mesh.indices.push_back(mesh.indices[index]);
            mesh.indices.push_back(mesh.indices[index + 2U]);
            mesh.indices.push_back(mesh.indices[index + 1U]);
        }
        return mesh;
    }

    static std::shared_ptr<Model> create_model_data()
    {
        Material material = {};
        material.textures.set("diffuse", not_found_texture);
        return std::make_shared<Model>(create_two_sided_fallback_mesh(), material);
    }

    AssetPromise<Model> load_model_async(const std::filesystem::path& asset_path)
    {
        auto asset = create_model_data();
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            AssetPromise<Model> result = {};
            result.asset = asset;
            warn_missing_dispatcher("load a model asynchronously");
            result.promise = make_missing_dispatcher_future("load a model asynchronously");
            return result;
        }

        LoadModelRequest message(asset_path, asset.get());
        message.not_handled_behavior = MessageNotHandledBehavior::WARN;
        auto future = dispatcher->post(message);
        AssetPromise<Model> result = {};
        result.asset = asset;
        result.promise = future;
        return result;
    }

    std::shared_ptr<Model> load_model(const std::filesystem::path& asset_path)
    {
        auto asset = create_model_data();
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            warn_missing_dispatcher("load a model synchronously");
            return asset;
        }

        LoadModelRequest message(asset_path, asset.get());
        message.not_handled_behavior = MessageNotHandledBehavior::WARN;
        dispatcher->send(message);
        return asset;
    }
}
