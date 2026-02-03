#pragma once
#include "tbx/assets/assets.h"
#include "tbx/graphics/material.h"
#include "tbx/tbx_api.h"
#include <memory>

namespace tbx
{
    /// <summary>
    /// Purpose: Begins loading a material asynchronously via the global message dispatcher.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns an AssetPromise that shares ownership of the material with the caller.
    /// Thread Safety: Safe to call concurrently provided the global dispatcher is thread-safe.
    /// </remarks>
    TBX_API AssetPromise<Material> load_material_async(
        const std::filesystem::path& asset_path);

    /// <summary>
    /// Purpose: Loads a material synchronously via the global message dispatcher.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns shared material data owned by the caller.
    /// Thread Safety: Safe to call concurrently provided the global dispatcher is thread-safe.
    /// </remarks>
    TBX_API std::shared_ptr<Material> load_material(
        const std::filesystem::path& asset_path);
}
