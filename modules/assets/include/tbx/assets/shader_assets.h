#pragma once
#include "tbx/assets/assets.h"
#include "tbx/graphics/shader.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <memory>

namespace tbx
{
    /// <summary>
    /// Purpose: Begins loading a shader program asynchronously via the global message dispatcher.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns an AssetPromise that shares ownership of the shader program with
    /// the caller. The payload is destroyed when the final shared reference is released.
    /// Thread Safety: Safe to call concurrently provided the global dispatcher is thread-safe.
    /// </remarks>
    TBX_API AssetPromise<Shader> load_shader_async(
        const std::filesystem::path& asset_path);

    /// <summary>
    /// Purpose: Loads a shader program synchronously via the global message dispatcher.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns shared shader program data owned by the caller. The payload is
    /// destroyed when the final shared reference is released.
    /// Thread Safety: Safe to call concurrently provided the global dispatcher is thread-safe.
    /// </remarks>
    TBX_API std::shared_ptr<Shader> load_shader(
        const std::filesystem::path& asset_path);
}
