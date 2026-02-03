#include "tbx/assets/material_assets.h"
#include "tbx/assets/messages.h"
#include <memory>

namespace tbx
{
    static std::shared_ptr<Material> create_material_data(
        const std::shared_ptr<Material>& default_data)
    {
        if (default_data)
        {
            return std::make_shared<Material>(*default_data);
        }

        return std::make_shared<Material>();
    }

    AssetPromise<Material> load_material_async(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<Material>& default_data)
    {
        auto asset = create_material_data(default_data);
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
        message.not_handled_behavior = MessageNotHandledBehavior::Warn;
        auto future = dispatcher->post(message);
        AssetPromise<Material> result = {};
        result.asset = asset;
        result.promise = future;
        return result;
    }

    std::shared_ptr<Material> load_material(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<Material>& default_data)
    {
        auto asset = create_material_data(default_data);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            warn_missing_dispatcher("load a material synchronously");
            return asset;
        }

        LoadMaterialRequest message(
            asset_path,
            asset.get());
        message.not_handled_behavior = MessageNotHandledBehavior::Warn;
        dispatcher->send(message);
        return asset;
    }
}
