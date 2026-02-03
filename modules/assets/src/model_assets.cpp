#include "tbx/assets/model_assets.h"
#include "tbx/assets/messages.h"
#include <memory>

namespace tbx
{
    static std::shared_ptr<Model> create_model_data(
        const std::shared_ptr<Model>& default_data)
    {
        if (default_data)
        {
            return std::make_shared<Model>(*default_data);
        }

        return std::make_shared<Model>();
    }

    AssetPromise<Model> load_model_async(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<Model>& default_data)
    {
        auto asset = create_model_data(default_data);
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
        message.not_handled_behavior = MessageNotHandledBehavior::Warn;
        auto future = dispatcher->post(message);
        AssetPromise<Model> result = {};
        result.asset = asset;
        result.promise = future;
        return result;
    }

    std::shared_ptr<Model> load_model(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<Model>& default_data)
    {
        auto asset = create_model_data(default_data);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            warn_missing_dispatcher("load a model synchronously");
            return asset;
        }

        LoadModelRequest message(
            asset_path,
            asset.get());
        message.not_handled_behavior = MessageNotHandledBehavior::Warn;
        dispatcher->send(message);
        return asset;
    }
}
