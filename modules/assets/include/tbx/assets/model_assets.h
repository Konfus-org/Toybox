#pragma once
#include "tbx/assets/assets.h"
#include "tbx/graphics/model.h"
#include "tbx/tbx_api.h"
#include <memory>

namespace tbx
{
    /// <summary>
    /// Purpose: Begins loading a model asynchronously via the global message dispatcher.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns an AssetPromise that shares ownership of the model wrapper with
    /// the caller. The payload is destroyed when the final shared reference is released.
    /// Thread Safety: Safe to call concurrently provided the global dispatcher is thread-safe.
    /// </remarks>
    TBX_API AssetPromise<Model> load_model_async(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<Model>& default_data = {});

    /// <summary>
    /// Purpose: Loads a model synchronously via the global message dispatcher.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns a shared model wrapper owned by the caller. The payload is destroyed
    /// when the final shared reference is released.
    /// Thread Safety: Safe to call concurrently provided the global dispatcher is thread-safe.
    /// </remarks>
    TBX_API std::shared_ptr<Asset<Model>> load_model(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<Model>& default_data = {});
}
