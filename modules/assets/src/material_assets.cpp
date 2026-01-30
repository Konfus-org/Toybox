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
            return {};
        }

        auto future = dispatcher->post<LoadMaterialRequest>(
            asset_path,
            asset.get());
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
            return {};
        }

        LoadMaterialRequest message(
            asset_path,
            asset.get());
        dispatcher->send(message);
        return asset;
    }
}
