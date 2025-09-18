#pragma once
#include "Tbx/Ids/GUID.h"
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <typeindex>
#include <utility>

namespace Tbx
{
    /// <summary>
    /// Represents the current loading state for a tracked asset.
    /// </summary>
    enum class AssetStatus
    {
        /// <summary>An asset has been registered but no load has been requested.</summary>
        Unloaded,
        /// <summary>A load has been requested and is in progress.</summary>
        Loading,
        /// <summary>The asset has successfully loaded and is ready for consumption.</summary>
        Loaded,
        /// <summary>The asset failed to load.</summary>
        Failed
    };

    /// <summary>
    /// Identifies a registered asset and tracks its lifecycle state.
    /// </summary>
    struct AssetHandle
    {
        AssetHandle() = default;

        /// <summary>
        /// Constructs a handle with the provided name and optional identifier.
        /// </summary>
        explicit AssetHandle(std::string name, Guid id = Guid::Generate())
            : Name(std::move(name)), Id(std::move(id)) {}

        /// <summary>Human readable name for diagnostics.</summary>
        std::string Name = "";
        /// <summary>Current status for the associated asset.</summary>
        AssetStatus Status = AssetStatus::Unloaded;
        /// <summary>Unique identifier for quick lookup by handle.</summary>
        Guid Id = Guid::Generate();
    };

    /// <summary>
    /// Internal bookkeeping structure used by <see cref="AssetServer"/> to track assets.
    /// </summary>
    struct AssetRecord
    {
        /// <summary>Handle exposed publicly for asset consumers.</summary>
        AssetHandle Handle = {};
        /// <summary>Absolute file system path for the asset on disk.</summary>
        std::filesystem::path AbsolutePath = {};
        /// <summary>Normalized path used as the lookup key within the server.</summary>
        std::string NormalizedPath = {};
        /// <summary>The concrete type stored within <see cref="Data"/>.</summary>
        std::type_index Type = std::type_index(typeid(void));
        /// <summary>Shared pointer to the placeholder or fully loaded asset data.</summary>
        std::shared_ptr<void> Data = nullptr;
        /// <summary>Future representing an in-flight asynchronous load operation.</summary>
        std::shared_future<void> LoadingTask = {};
        /// <summary>Shared future keeping the asynchronous worker alive while it loads.</summary>
        std::shared_future<void> ActiveAsyncTask = {};
        /// <summary>Promise used to signal completion to threads waiting on synchronous loads.</summary>
        std::shared_ptr<std::promise<void>> LoadingPromise = {};
        /// <summary>Mutex guarding modifications to the record state.</summary>
        mutable std::mutex Mutex;
    };
}
