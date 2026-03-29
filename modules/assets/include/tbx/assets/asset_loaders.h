#pragma once
#include "tbx/assets/assets.h"
#include "tbx/assets/audio_assets.h"
#include "tbx/assets/material_assets.h"
#include "tbx/assets/model_assets.h"
#include "tbx/assets/shader_assets.h"
#include "tbx/assets/texture_assets.h"

namespace tbx
{
    /// @brief
    /// Purpose: Selects the loader endpoints for a given asset type.
    /// @details
    /// Ownership: Returns AssetPromise or shared asset pointers owned by the caller.
    /// Thread Safety: Safe to call concurrently; delegates to thread-safe loaders.

    template <typename TAsset>
    struct AssetLoader
    {
        static AssetPromise<TAsset> load_async(const std::filesystem::path& asset_path)
        {
            static_assert(sizeof(TAsset) == 0, "No async asset loader registered for this type.");
            return load_async(asset_path);
        }

        static std::shared_ptr<TAsset> load(const std::filesystem::path& asset_path)
        {
            static_assert(sizeof(TAsset) == 0, "No asset loader registered for this type.");
            return load(asset_path);
        }
    };

    /// @brief
    /// Purpose: Provides the async loader specialization for Model assets.
    /// @details
    /// Ownership: Returns an AssetPromise that shares ownership with the caller.
    /// Thread Safety: Safe to call concurrently.

    template <>
    struct AssetLoader<Model>
    {
        static AssetPromise<Model> load_async(const std::filesystem::path& asset_path)
        {
            return load_model_async(asset_path);
        }

        static std::shared_ptr<Model> load(const std::filesystem::path& asset_path)
        {
            return load_model(asset_path);
        }
    };

    /// @brief
    /// Purpose: Provides the async loader specialization for Texture assets.
    /// @details
    /// Ownership: Returns an AssetPromise that shares ownership with the caller.
    /// Thread Safety: Safe to call concurrently.

    template <>
    struct AssetLoader<Texture>
    {
        static AssetPromise<Texture> load_async(const std::filesystem::path& asset_path)
        {
            return load_texture_async(
                asset_path,
                TextureWrap::REPEAT,
                TextureFilter::LINEAR,
                TextureFormat::RGBA,
                TextureMipmaps::ENABLED,
                TextureCompression::AUTO);
        }

        static std::shared_ptr<Texture> load(const std::filesystem::path& asset_path)
        {
            return load_texture(
                asset_path,
                TextureWrap::REPEAT,
                TextureFilter::LINEAR,
                TextureFormat::RGBA,
                TextureMipmaps::ENABLED,
                TextureCompression::AUTO);
        }
    };

    /// @brief
    /// Purpose: Provides the async loader specialization for AudioClip assets.
    /// @details
    /// Ownership: Returns an AssetPromise that shares ownership with the caller.
    /// Thread Safety: Safe to call concurrently.

    template <>
    struct AssetLoader<AudioClip>
    {
        static AssetPromise<AudioClip> load_async(const std::filesystem::path& asset_path)
        {
            return load_audio_async(asset_path);
        }

        static std::shared_ptr<AudioClip> load(const std::filesystem::path& asset_path)
        {
            return load_audio(asset_path);
        }
    };

    /// @brief
    /// Purpose: Provides the async loader specialization for Shader assets.
    /// @details
    /// Ownership: Returns an AssetPromise that shares ownership with the caller.
    /// Thread Safety: Safe to call concurrently.

    template <>
    struct AssetLoader<Shader>
    {
        static AssetPromise<Shader> load_async(const std::filesystem::path& asset_path)
        {
            return load_shader_async(asset_path);
        }

        static std::shared_ptr<Shader> load(const std::filesystem::path& asset_path)
        {
            return load_shader(asset_path);
        }
    };

    /// @brief
    /// Purpose: Provides the async loader specialization for Material assets.
    /// @details
    /// Ownership: Returns an AssetPromise that shares ownership with the caller.
    /// Thread Safety: Safe to call concurrently.

    template <>
    struct AssetLoader<Material>
    {
        static AssetPromise<Material> load_async(const std::filesystem::path& asset_path)
        {
            return load_material_async(asset_path);
        }

        static std::shared_ptr<Material> load(const std::filesystem::path& asset_path)
        {
            return load_material(asset_path);
        }
    };
}
