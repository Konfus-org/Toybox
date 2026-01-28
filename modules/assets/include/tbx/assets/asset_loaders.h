#pragma once
#include "tbx/assets/assets.h"
#include "tbx/assets/audio_assets.h"
#include "tbx/assets/model_assets.h"
#include "tbx/assets/texture_assets.h"

namespace tbx
{
    /// <summary>
    /// Purpose: Selects the asynchronous loader for a given asset type.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns an AssetPromise that shares ownership with the caller.
    /// Thread Safety: Safe to call concurrently; delegates to thread-safe loaders.
    /// </remarks>
    template <typename TAsset>
    struct AssetAsyncLoader
    {
        static AssetPromise<TAsset> load(const std::filesystem::path& asset_path)
        {
            static_assert(sizeof(TAsset) == 0, "No async asset loader registered for this type.");
            return load(asset_path);
        }
    };

    /// <summary>
    /// Purpose: Provides the async loader specialization for Model assets.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns an AssetPromise that shares ownership with the caller.
    /// Thread Safety: Safe to call concurrently.
    /// </remarks>
    template <>
    struct AssetAsyncLoader<Model>
    {
        static AssetPromise<Model> load(const std::filesystem::path& asset_path)
        {
            return load_model_async(asset_path);
        }
    };

    /// <summary>
    /// Purpose: Provides the async loader specialization for Texture assets.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns an AssetPromise that shares ownership with the caller.
    /// Thread Safety: Safe to call concurrently.
    /// </remarks>
    template <>
    struct AssetAsyncLoader<Texture>
    {
        static AssetPromise<Texture> load(const std::filesystem::path& asset_path)
        {
            return load_texture_async(asset_path);
        }
    };

    /// <summary>
    /// Purpose: Provides the async loader specialization for AudioClip assets.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns an AssetPromise that shares ownership with the caller.
    /// Thread Safety: Safe to call concurrently.
    /// </remarks>
    template <>
    struct AssetAsyncLoader<AudioClip>
    {
        static AssetPromise<AudioClip> load(const std::filesystem::path& asset_path)
        {
            return load_audio_async(asset_path);
        }
    };
}
