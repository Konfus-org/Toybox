#include "tbx/assets/model_assets.h"
#include "tbx/assets/messages.h"

namespace tbx
{
    static void destroy_model_payload(Model* payload)
    {
        delete payload;
    }

    static std::shared_ptr<Model> create_model_payload(
        const std::shared_ptr<Model>& default_data)
    {
        if (default_data)
        {
            return std::shared_ptr<Model>(
                new Model(*default_data),
                &destroy_model_payload);
        }

        return std::shared_ptr<Model>(
            new Model(),
            &destroy_model_payload);
    }

    AssetPromise<Model> load_model_async(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<Model>& default_data)
    {
        auto asset = create_model_payload(default_data);
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

    std::shared_ptr<Model> load_model(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<Model>& default_data)
    {
        auto asset = create_model_payload(default_data);
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
