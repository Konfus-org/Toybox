#pragma once
#include "tbx/assets/assets.h"
#include "tbx/graphics/model.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <memory>

namespace tbx
{
    /// <summary>
    /// Purpose: Begins loading a model asynchronously via the global message dispatcher.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns an AssetPromise that shares ownership of the model data with
    /// the caller. The payload is destroyed when the final shared reference is released.
    /// Thread Safety: Safe to call concurrently provided the global dispatcher is thread-safe.
    /// </remarks>
    TBX_API AssetPromise<Model> load_model_async(
        const std::filesystem::path& asset_path);

    /// <summary>
    /// Purpose: Loads a model synchronously via the global message dispatcher.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns shared model data owned by the caller. The payload is destroyed when
    /// the final shared reference is released.
    /// Thread Safety: Safe to call concurrently provided the global dispatcher is thread-safe.
    /// </remarks>
    TBX_API std::shared_ptr<Model> load_model(
        const std::filesystem::path& asset_path);
}
