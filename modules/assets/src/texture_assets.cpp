#include "tbx/assets/texture_assets.h"
#include "tbx/assets/messages.h"

namespace tbx
{
    static void destroy_texture_payload(Texture* payload)
    {
        delete payload;
    }

    static std::shared_ptr<Texture> create_texture_payload(
        const std::shared_ptr<Texture>& default_data)
    {
        if (default_data)
        {
            return std::shared_ptr<Texture>(
                new Texture(*default_data),
                &destroy_texture_payload);
        }

        return std::shared_ptr<Texture>(
            new Texture(),
            &destroy_texture_payload);
    }

    AssetPromise<Texture> load_texture_async(
        const std::filesystem::path& asset_path,
        TextureWrap wrap,
        TextureFilter filter,
        TextureFormat format,
        const std::shared_ptr<Texture>& default_data)
    {
        auto asset = create_texture_payload(default_data);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            AssetPromise<Texture> result = {};
            result.asset = asset;
            result.promise = make_missing_dispatcher_future("load a texture asynchronously");
            return result;
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

    std::shared_ptr<Texture> load_texture(
        const std::filesystem::path& asset_path,
        TextureWrap wrap,
        TextureFilter filter,
        TextureFormat format,
        const std::shared_ptr<Texture>& default_data)
    {
        auto asset = create_texture_payload(default_data);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            dispatcher_missing_result("load a texture synchronously");
            return asset;
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
