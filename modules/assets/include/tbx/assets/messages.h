#pragma once
#include "tbx/messages/message.h"
#include "tbx/graphics/texture.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <shared_mutex>
#include <mutex>

namespace tbx
{
    struct Texture;
    struct Model;
    struct AudioClip;

    /// <summary>
    /// Purpose: Base message requesting that an asset payload be loaded.
    /// </summary>
    /// <remarks>
    /// Ownership: The asset pointer is non-owning and owned by the caller.
    /// Thread Safety: Message dispatch follows the dispatcher contract.
    /// </remarks>
    template <typename TAsset>
    struct LoadAssetRequest : public Request<void>
    {
        LoadAssetRequest(
            const std::filesystem::path& asset_path,
            TAsset* asset_payload)
            : path(asset_path)
            , asset(asset_payload)
        {
        }

        std::filesystem::path path;
        TAsset* asset = nullptr;

        /// <summary>
        /// Purpose: Executes a read operation against the asset payload under a shared lock.
        /// </summary>
        /// <remarks>
        /// Ownership: The callback receives a non-owning pointer; no ownership transfer occurs.
        /// Thread Safety: Acquires a shared lock for the duration of the callback.
        /// </remarks>
        template <typename TFunc>
        void with_asset_read(TFunc&& func) const
        {
            std::shared_lock lock(_asset_mutex);
            func(asset);
        }

        /// <summary>
        /// Purpose: Executes a write operation against the asset payload under an exclusive lock.
        /// </summary>
        /// <remarks>
        /// Ownership: The callback receives a non-owning pointer; no ownership transfer occurs.
        /// Thread Safety: Acquires an exclusive lock for the duration of the callback.
        /// </remarks>
        template <typename TFunc>
        void with_asset_write(TFunc&& func)
        {
            std::unique_lock lock(_asset_mutex);
            func(asset);
        }

      private:
        mutable std::shared_mutex _asset_mutex;
    };

    /// <summary>
    /// Purpose: Message requesting that a texture payload be loaded with specific settings.
    /// </summary>
    /// <remarks>
    /// Ownership: The texture pointer is non-owning and owned by the caller.
    /// Thread Safety: Message dispatch follows the dispatcher contract; asset mutations must
    /// be synchronized through Texture accessors.
    /// </remarks>
    struct TBX_API LoadTextureRequest : public LoadAssetRequest<Texture>
    {
        LoadTextureRequest(
            const std::filesystem::path& asset_path,
            Texture* asset_payload,
            TextureWrap wrap,
            TextureFilter filter,
            TextureFormat format)
            : LoadAssetRequest<Texture>(asset_path, asset_payload)
            , wrap(wrap)
            , filter(filter)
            , format(format)
        {
        }

        TextureWrap wrap = TextureWrap::Repeat;
        TextureFilter filter = TextureFilter::Nearest;
        TextureFormat format = TextureFormat::RGBA;
    };

    using LoadModelRequest = LoadAssetRequest<Model>;
    using LoadAudioRequest = LoadAssetRequest<AudioClip>;
}
