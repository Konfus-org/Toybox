#pragma once
#include "tbx/audio/audio_clip.h"
#include "tbx/common/result.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/model.h"
#include "tbx/graphics/shader.h"
#include "tbx/graphics/texture.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <future>
#include <memory>
#include <string_view>
#include <utility>

namespace tbx
{
    template <typename TAsset>
    struct AssetPromise;

    template <typename TAsset>
    struct AssetLoader;

    /// @brief
    /// Purpose: Represents an asynchronous asset load with a ready promise.
    /// @details
    /// Ownership: The asset data is shared between the caller and the asset system. The promise is
    /// shared and can be waited on by multiple callers. Thread Safety: Safe to copy between
    /// threads; coordinate asset mutation externally.
    /// @brief
    /// Purpose: Logs a warning when no global dispatcher is available.
    /// @details
    /// Ownership: Does not transfer ownership.
    /// Thread Safety: Safe to call concurrently.
    TBX_API void warn_missing_dispatcher(std::string_view action);

    /// @brief
    /// Purpose: Creates a shared future that completes with a missing-dispatcher failure.
    /// @details
    /// Ownership: The returned future owns its shared state and completes with a failed Result.
    /// Thread Safety: Safe to call concurrently.
    TBX_API std::shared_future<Result> make_missing_dispatcher_future(std::string_view action);

    /// @brief
    /// Purpose: Begins loading a model asynchronously via the global message dispatcher.
    /// @details
    /// Ownership: Returns an AssetPromise that shares ownership of the model data with the caller.
    /// The payload is destroyed when the final shared reference is released. Thread Safety: Safe to
    /// call concurrently provided the global dispatcher is thread-safe.
    TBX_API AssetPromise<Model> load_model_async(const std::filesystem::path& asset_path);

    /// @brief
    /// Purpose: Loads a model synchronously via the global message dispatcher.
    /// @details
    /// Ownership: Returns shared model data owned by the caller. The payload is destroyed when the
    /// final shared reference is released. Thread Safety: Safe to call concurrently provided the
    /// global dispatcher is thread-safe.
    TBX_API std::shared_ptr<Model> load_model(const std::filesystem::path& asset_path);

    /// @brief
    /// Purpose: Begins loading a texture asynchronously via the global message dispatcher.
    /// @details
    /// Ownership: Returns an AssetPromise that shares ownership of the texture data with the
    /// caller. The payload is destroyed when the final shared reference is released. Thread Safety:
    /// Safe to call concurrently provided the global dispatcher is thread-safe.
    TBX_API AssetPromise<Texture> load_texture_async(
        const std::filesystem::path& asset_path,
        TextureWrap wrap,
        TextureFilter filter,
        TextureFormat format,
        TextureMipmaps mipmaps = TextureMipmaps::ENABLED,
        TextureCompression compression = TextureCompression::AUTO);

    /// @brief
    /// Purpose: Loads a texture synchronously via the global message dispatcher.
    /// @details
    /// Ownership: Returns shared texture data owned by the caller. The payload is destroyed when
    /// the final shared reference is released. Thread Safety: Safe to call concurrently provided
    /// the global dispatcher is thread-safe.
    TBX_API std::shared_ptr<Texture> load_texture(
        const std::filesystem::path& asset_path,
        TextureWrap wrap,
        TextureFilter filter,
        TextureFormat format,
        TextureMipmaps mipmaps = TextureMipmaps::ENABLED,
        TextureCompression compression = TextureCompression::AUTO);

    /// @brief
    /// Purpose: Begins loading audio asynchronously via the global message dispatcher.
    /// @details
    /// Ownership: Returns an AssetPromise that shares ownership of the audio data with the caller.
    /// The payload is destroyed when the final shared reference is released. Thread Safety: Safe to
    /// call concurrently provided the global dispatcher is thread-safe.
    TBX_API AssetPromise<AudioClip> load_audio_async(const std::filesystem::path& asset_path);

    /// @brief
    /// Purpose: Loads audio synchronously via the global message dispatcher.
    /// @details
    /// Ownership: Returns shared audio data owned by the caller. The payload is destroyed when the
    /// final shared reference is released. Thread Safety: Safe to call concurrently provided the
    /// global dispatcher is thread-safe.
    TBX_API std::shared_ptr<AudioClip> load_audio(const std::filesystem::path& asset_path);

    /// @brief
    /// Purpose: Begins loading a shader program asynchronously via the global message dispatcher.
    /// @details
    /// Ownership: Returns an AssetPromise that shares ownership of the shader program with the
    /// caller. The payload is destroyed when the final shared reference is released. Thread Safety:
    /// Safe to call concurrently provided the global dispatcher is thread-safe.
    TBX_API AssetPromise<Shader> load_shader_async(const std::filesystem::path& asset_path);

    /// @brief
    /// Purpose: Loads a shader program synchronously via the global message dispatcher.
    /// @details
    /// Ownership: Returns shared shader program data owned by the caller. The payload is destroyed
    /// when the final shared reference is released. Thread Safety: Safe to call concurrently
    /// provided the global dispatcher is thread-safe.
    TBX_API std::shared_ptr<Shader> load_shader(const std::filesystem::path& asset_path);

    /// @brief
    /// Purpose: Begins loading a material asynchronously via the global message dispatcher.
    /// @details
    /// Ownership: Returns an AssetPromise that shares ownership of the material with the caller.
    /// Thread Safety: Safe to call concurrently provided the global dispatcher is thread-safe.
    TBX_API AssetPromise<Material> load_material_async(const std::filesystem::path& asset_path);

    /// @brief
    /// Purpose: Loads a material synchronously via the global message dispatcher.
    /// @details
    /// Ownership: Returns shared material data owned by the caller.
    /// Thread Safety: Safe to call concurrently provided the global dispatcher is thread-safe.
    TBX_API std::shared_ptr<Material> load_material(const std::filesystem::path& asset_path);

    /// @brief
    /// Purpose: Selects the loader endpoints for a given asset type.
    /// @details
    /// Ownership: Returns AssetPromise or shared asset pointers owned by the caller.
    /// Thread Safety: Safe to call concurrently; delegates to thread-safe loaders.
}

#include "tbx/assets/asset_loaders.inl"
