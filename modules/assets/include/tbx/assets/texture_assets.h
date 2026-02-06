#pragma once
#include "tbx/assets/assets.h"
#include "tbx/graphics/texture.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <memory>

namespace tbx
{
    /// <summary>
    /// Purpose: Begins loading a texture asynchronously via the global message dispatcher.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns an AssetPromise that shares ownership of the texture data with
    /// the caller. The payload is destroyed when the final shared reference is released.
    /// Thread Safety: Safe to call concurrently provided the global dispatcher is thread-safe.
    /// </remarks>
    TBX_API AssetPromise<Texture> load_texture_async(
        const std::filesystem::path& asset_path,
        TextureWrap wrap,
        TextureFilter filter,
        TextureFormat format);

    /// <summary>
    /// Purpose: Loads a texture synchronously via the global message dispatcher.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns shared texture data owned by the caller. The payload is destroyed when
    /// the final shared reference is released.
    /// Thread Safety: Safe to call concurrently provided the global dispatcher is thread-safe.
    /// </remarks>
    TBX_API std::shared_ptr<Texture> load_texture(
        const std::filesystem::path& asset_path,
        TextureWrap wrap,
        TextureFilter filter,
        TextureFormat format);
}
