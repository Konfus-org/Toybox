#pragma once
#include "tbx/common/result.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/tbx_api.h"
#include <future>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string_view>
#include <utility>

namespace tbx
{
    /// <summary>
    /// Purpose: Thread-safe asset wrapper that owns a payload and coordinates access.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the payload through a unique pointer and is typically shared via
    /// `std::shared_ptr`.
    /// Thread Safety: Provides shared/exclusive accessors guarded by an internal mutex.
    /// </remarks>
    template <typename TAsset>
    class Asset final
    {
      public:
        explicit Asset(std::unique_ptr<TAsset> payload)
            : _payload(std::move(payload))
        {
        }

        template <typename TFunc>
        decltype(auto) read(TFunc&& func) const
        {
            std::shared_lock lock(_mutex);
            return std::forward<TFunc>(func)(_payload.get());
        }

        template <typename TFunc>
        decltype(auto) write(TFunc&& func)
        {
            std::unique_lock lock(_mutex);
            return std::forward<TFunc>(func)(_payload.get());
        }

      private:
        std::unique_ptr<TAsset> _payload;
        mutable std::shared_mutex _mutex;
    };

    /// <summary>
    /// Purpose: Represents an asynchronous asset load with a ready promise.
    /// </summary>
    /// <remarks>
    /// Ownership: The asset wrapper is shared between the caller and the asset system.
    /// The promise is shared and can be waited on by multiple callers.
    /// Thread Safety: Safe to copy between threads; use Asset accessors for payload access.
    /// </remarks>
    template <typename TAsset>
    struct AssetPromise
    {
        std::shared_ptr<Asset<TAsset>> asset;
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
