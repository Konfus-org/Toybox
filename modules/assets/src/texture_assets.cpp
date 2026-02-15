#include "tbx/assets/texture_assets.h"
#include "tbx/assets/messages.h"
#include "tbx/debugging/macros.h"
#include <memory>

namespace tbx
{
    static std::shared_ptr<Texture> create_fallback_texture(
        TextureWrap wrap,
        TextureFilter filter,
        TextureFormat format,
        TextureMipmaps mipmaps,
        TextureCompression compression)
    {
        auto pixels = std::vector<Pixel>();
        if (format == TextureFormat::RGB)
            pixels = {255, 0, 255};
        else
            pixels = {255, 0, 255, 255};

        return std::make_shared<Texture>(
            Size(1, 1),
            wrap,
            filter,
            format,
            mipmaps,
            compression,
            pixels);
    }

    AssetPromise<Texture> load_texture_async(
        const std::filesystem::path& asset_path,
        TextureWrap wrap,
        TextureFilter filter,
        TextureFormat format,
        TextureMipmaps mipmaps,
        TextureCompression compression)
    {
        auto asset = create_fallback_texture(wrap, filter, format, mipmaps, compression);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            AssetPromise<Texture> result = {};
            result.asset = asset;
            warn_missing_dispatcher("load a texture asynchronously");
            result.promise = make_missing_dispatcher_future("load a texture asynchronously");
            return result;
        }

        LoadTextureRequest message(
            asset_path,
            asset.get(),
            wrap,
            filter,
            format,
            mipmaps,
            compression);
        message.not_handled_behavior = MessageNotHandledBehavior::WARN;
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
        TextureMipmaps mipmaps,
        TextureCompression compression)
    {
        auto asset = create_fallback_texture(wrap, filter, format, mipmaps, compression);
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
            format,
            mipmaps,
            compression);
        message.not_handled_behavior = MessageNotHandledBehavior::WARN;
        auto result = dispatcher->send(message);
        if (!result.succeeded())
        {
            TBX_TRACE_WARNING(
                "Texture load request failed for '{}': {}. Using fallback pink texture.",
                asset_path.string(),
                result.get_report());
        }
        return asset;
    }
}
