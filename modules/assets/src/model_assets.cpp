#include "tbx/assets/model_assets.h"
#include "tbx/assets/messages.h"
#include <memory>

namespace tbx
{
    static std::unique_ptr<Model> create_model_data(
        const std::shared_ptr<Model>& default_data)
    {
        if (default_data)
        {
            return std::make_unique<Model>(*default_data);
        }

        return std::make_unique<Model>();
    }

    static std::shared_ptr<Asset<Model>> create_model_asset(
        const std::shared_ptr<Model>& default_data)
    {
        return std::make_shared<Asset<Model>>(create_model_data(default_data));
    }

    AssetPromise<Model> load_model_async(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<Model>& default_data)
    {
        auto asset = create_model_asset(default_data);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            throw_missing_dispatcher("load a model asynchronously");
        }

        auto future = dispatcher->post<LoadModelRequest>(
            asset_path,
            asset.get());
        AssetPromise<Model> result = {};
        result.asset = asset;
        result.promise = future;
        return result;
    }

    std::shared_ptr<Asset<Model>> load_model(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<Model>& default_data)
    {
        auto asset = create_model_asset(default_data);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            throw_missing_dispatcher("load a model synchronously");
        }

        LoadModelRequest message(
            asset_path,
            asset.get());
        dispatcher->send(message);
        return asset;
    }
}
