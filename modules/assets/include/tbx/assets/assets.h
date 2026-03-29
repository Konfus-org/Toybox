#pragma once
#include "tbx/common/result.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/tbx_api.h"
#include <future>
#include <memory>
#include <string_view>
#include <utility>

namespace tbx
{
    /// @brief
    /// Purpose: Represents an asynchronous asset load with a ready promise.
    /// @details
    /// Ownership: The asset data is shared between the caller and the asset system. The promise is
    /// shared and can be waited on by multiple callers. Thread Safety: Safe to copy between
    /// threads; coordinate asset mutation externally.
    template <typename TAsset>
    struct AssetPromise
    {
        std::shared_ptr<TAsset> asset;
        std::shared_future<Result> promise;
    };

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
}
