#pragma once
#include "tbx/common/result.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/tbx_api.h"
#include <future>
#include <memory>
#include <string_view>

namespace tbx
{
    /// <summary>
    /// Purpose: Represents an asynchronous asset load with a ready promise.
    /// </summary>
    /// <remarks>
    /// Ownership: The asset is shared between the caller and the asset system. The
    /// promise is shared and can be waited on by multiple callers.
    /// Thread Safety: Safe to copy between threads; asset access follows payload rules.
    /// </remarks>
    template <typename TAsset>
    struct AssetPromise
    {
        std::shared_ptr<TAsset> asset;
        std::shared_future<Result> promise;
    };

    /// <summary>
    /// Purpose: Throws when no global dispatcher is available.
    /// </summary>
    /// <remarks>
    /// Ownership: Throws on failure; no ownership transfer occurs.
    /// Thread Safety: Safe to call concurrently; throws on missing dispatcher.
    /// </remarks>
    TBX_API void throw_missing_dispatcher(std::string_view action);

    /// <summary>
    /// Purpose: Creates a shared future that surfaces a missing-dispatcher exception.
    /// </summary>
    /// <remarks>
    /// Ownership: The returned future owns its shared state.
    /// Thread Safety: Safe to call concurrently.
    /// </remarks>
    TBX_API std::shared_future<Result> make_missing_dispatcher_future(std::string_view action);
}
