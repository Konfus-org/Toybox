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

    static std::shared_ptr<Asset<Texture>> create_texture_asset(
        const std::shared_ptr<Texture>& default_data)
    {
        return std::make_shared<Asset<Texture>>(create_texture_data(default_data));
    }

    AssetPromise<Texture> load_texture_async(
        const std::filesystem::path& asset_path,
        TextureWrap wrap,
        TextureFilter filter,
        TextureFormat format,
        const std::shared_ptr<Texture>& default_data)
    {
        auto asset = create_texture_asset(default_data);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            throw_missing_dispatcher("load a texture asynchronously");
        }

        auto future = dispatcher->post<LoadTextureRequest>(
            asset_path,
            asset.get(),
            wrap,
            filter,
            format);
        AssetPromise<Texture> result = {};
        result.asset = asset;
        result.promise = future;
        return result;
    }

    std::shared_ptr<Asset<Texture>> load_texture(
        const std::filesystem::path& asset_path,
        TextureWrap wrap,
        TextureFilter filter,
        TextureFormat format,
        const std::shared_ptr<Texture>& default_data)
    {
        auto asset = create_texture_asset(default_data);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            throw_missing_dispatcher("load a texture synchronously");
        }

        LoadTextureRequest message(
            asset_path,
            asset.get(),
            wrap,
            filter,
            format);
        dispatcher->send(message);
        return asset;
    }
}
