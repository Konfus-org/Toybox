#include "tbx/assets/texture_assets.h"
#include "tbx/assets/messages.h"
#include <memory>

namespace tbx
{
    static std::shared_ptr<Texture> create_texture_data(
        const std::shared_ptr<Texture>& default_data)
    {
        if (default_data)
        {
            return std::make_shared<Texture>(*default_data);
        }

        return std::make_shared<Texture>();
    }

    AssetPromise<Texture> load_texture_async(
        const std::filesystem::path& asset_path,
        TextureWrap wrap,
        TextureFilter filter,
        TextureFormat format,
        const std::shared_ptr<Texture>& default_data)
    {
        auto asset = create_texture_data(default_data);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            AssetPromise<Texture> result = {};
            result.asset = asset;
            warn_missing_dispatcher("load a texture asynchronously");
            result.promise = make_missing_dispatcher_future("load a texture asynchronously");
            return result;
        }

        LoadTextureRequest message(asset_path, asset.get(), wrap, filter, format);
        message.not_handled_behavior = MessageNotHandledBehavior::Warn;
        auto future = dispatcher->post(message);
        AssetPromise<Texture> result = {};
        result.asset = asset;
        result.promise = future;
        return result;
    }

    std::shared_ptr<Texture> load_texture(
        const std::filesystem::path& asset_path,
        TextureWrap wrap,
        TextureFilter filter,
        TextureFormat format,
        const std::shared_ptr<Texture>& default_data)
    {
        auto asset = create_texture_data(default_data);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            warn_missing_dispatcher("load a texture synchronously");
            return asset;
        }

        LoadTextureRequest message(
            asset_path,
            asset.get(),
            wrap,
            filter,
            format);
        message.not_handled_behavior = MessageNotHandledBehavior::Warn;
        dispatcher->send(message);
        return asset;
    }
}
