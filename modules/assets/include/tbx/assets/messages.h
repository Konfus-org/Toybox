#pragma once
#include "tbx/assets/assets.h"
#include "tbx/graphics/texture.h"
#include "tbx/messages/message.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <utility>

namespace tbx
{
    struct Texture;
    struct Model;
    struct AudioClip;

    /// <summary>
    /// Purpose: Base message requesting that an asset payload be loaded.
    /// </summary>
    /// <remarks>
    /// Ownership: The asset wrapper is shared between the caller and the message handler.
    /// Thread Safety: Payload access should use the provided read/write helpers.
    /// </remarks>
    template <typename TAsset>
    struct LoadAssetRequest : public Request<void>
    {
      public:
        LoadAssetRequest(
            const std::filesystem::path& asset_path,
            std::shared_ptr<Asset<TAsset>> asset_payload)
            : path(asset_path)
            , asset(std::move(asset_payload))
        {
            if (!asset)
            {
                throw std::invalid_argument("LoadAssetRequest requires a valid asset wrapper.");
            }
        }

        /// <summary>
        /// Purpose: Executes a read operation against the asset payload under a shared lock.
        /// </summary>
        /// <remarks>
        /// Ownership: The callback receives a non-owning pointer; no ownership transfer occurs.
        /// Thread Safety: Acquires a shared lock for the duration of the callback.
        /// </remarks>
        template <typename TFunc>
        decltype(auto) with_asset_read(TFunc&& func) const
        {
            return asset->with_read(std::forward<TFunc>(func));
        }

        /// <summary>
        /// Purpose: Executes a write operation against the asset payload under an exclusive lock.
        /// </summary>
        /// <remarks>
        /// Ownership: The callback receives a non-owning pointer; no ownership transfer occurs.
        /// Thread Safety: Acquires an exclusive lock for the duration of the callback.
        /// </remarks>
        template <typename TFunc>
        decltype(auto) with_asset_write(TFunc&& func)
        {
            return asset->with_write(std::forward<TFunc>(func));
        }

      public:
        std::filesystem::path path;
        std::shared_ptr<Asset<TAsset>> asset;
    };

    /// <summary>
    /// Purpose: Message requesting that a texture payload be loaded with specific settings.
    /// </summary>
    /// <remarks>
    /// Ownership: The texture asset wrapper is shared between the caller and the message handler.
    /// Thread Safety: Payload access should use the read/write helpers.
    /// </remarks>
    struct TBX_API LoadTextureRequest : public LoadAssetRequest<Texture>
    {
      public:
        LoadTextureRequest(
            const std::filesystem::path& asset_path,
            std::shared_ptr<Asset<Texture>> asset_payload,
            TextureWrap wrap,
            TextureFilter filter,
            TextureFormat format)
            : LoadAssetRequest<Texture>(asset_path, std::move(asset_payload))
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
