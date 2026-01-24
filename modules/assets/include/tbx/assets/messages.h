#pragma once
#include "tbx/assets/assets.h"
#include "tbx/graphics/texture.h"
#include "tbx/messages/message.h"
#include "tbx/tbx_api.h"
#include <filesystem>

namespace tbx
{
    struct Texture;
    struct Model;
    struct AudioClip;

    /// <summary>
    /// Purpose: Base message requesting that an asset payload be loaded.
    /// </summary>
    /// <remarks>
    /// Ownership: The asset wrapper pointer is non-owning and owned by the caller.
    /// Thread Safety: Payload access should use the Asset read/write accessors.
    /// </remarks>
    template <typename TAsset>
    struct LoadAssetRequest : public Request<void>
    {
      public:
        LoadAssetRequest(
            const std::filesystem::path& asset_path,
            Asset<TAsset>* asset_payload)
            : path(asset_path)
            , asset(asset_payload)
        {
        }

      public:
        std::filesystem::path path;
        Asset<TAsset>* asset = nullptr;
    };

    /// <summary>
    /// Purpose: Message requesting that a texture payload be loaded with specific settings.
    /// </summary>
    /// <remarks>
    /// Ownership: The texture asset wrapper pointer is non-owning and owned by the caller.
    /// Thread Safety: Payload access should use the Asset read/write accessors.
    /// </remarks>
    struct TBX_API LoadTextureRequest : public LoadAssetRequest<Texture>
    {
      public:
        LoadTextureRequest(
            const std::filesystem::path& asset_path,
            Asset<Texture>* asset_payload,
            TextureWrap wrap,
            TextureFilter filter,
            TextureFormat format)
            : LoadAssetRequest<Texture>(asset_path, asset_payload)
            , wrap(wrap)
            , filter(filter)
            , format(format)
        {
        }

      public:
        TextureWrap wrap = TextureWrap::Repeat;
        TextureFilter filter = TextureFilter::Nearest;
        TextureFormat format = TextureFormat::RGBA;
    };

    using LoadModelRequest = LoadAssetRequest<Model>;
    using LoadAudioRequest = LoadAssetRequest<AudioClip>;
}
