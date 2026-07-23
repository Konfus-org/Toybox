#pragma once
#include "tbx/api.h"
#include "tbx/assets/asset.h"
#include "tbx/assets/handle.h"
#include "tbx/events/events.h"
#include "tbx/files/watcher.h"
#include "tbx/jobs/jobs.h"
#include "tbx/utils/result.h"
#include "tbx/serialization/read_write.h"
#include "tbx/utils/typedefs.h"
#include "tbx/utils/uuid.h"
#include <any>
#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

// Async-first asset loading over the serializer registry (deserialize<T>), with
// .meta identity sidecars and watcher-driven hot reload. Paths resolve against the app's
// asset root first, then the engine's resources folder (TBX_RESOURCES_PATH) — engine-shipped
// models/textures/shaders are ordinary assets too. The state is runtime.assets, dropped
// (watcher included) when the RuntimeState dies.
// Thread safety: the identity and storage maps are mutex-guarded; load() decodes on a worker
// and stores on the main thread, load_now() decodes inline on the calling thread.
namespace tbx
{
    /// @brief
    /// Purpose: A handle after identity resolution: the id plus the path to decode from.
    struct TBX_DLL_EXPORT ResolvedAssetHandle
    {
        Uuid id = {};
        std::string relative_path = {};
    };

    /// @brief
    /// Purpose: One resident (decoded) asset plus its bookkeeping.
    struct TBX_DLL_EXPORT LoadedAsset
    {
        std::any data;
        std::chrono::steady_clock::time_point last_access = {};
    };

    /// @brief
    /// Purpose: The assets module's state, held by value on the Runtime — two maps and the
    /// glue around them: `assets` is the identity index (relative path -> uuid, filled from
    /// .meta sidecars) and `loaded_assets` is resident storage. The mutex guards both maps
    /// (loads run on workers); everything dies with the runtime, watcher included.
    struct TBX_DLL_EXPORT AssetsState
    {
        std::filesystem::path root = {};
        mutable std::mutex mutex;
        std::unordered_map<std::string, Uuid> assets;
        std::unordered_map<Uuid, LoadedAsset> loaded_assets;
        float idle_lifetime_seconds = 60.0f;
        std::chrono::steady_clock::time_point last_purge = std::chrono::steady_clock::now();
        std::optional<FileWatcher> watcher;
    };

    /// @brief
    /// Purpose: Sets the asset root and starts watching it for hot reload (change reports
    /// marshal through the jobs queue onto the main thread).
    TBX_DLL_EXPORT void set_asset_root(
        AssetsState& state,
        EventsState& events,
        JobsState& jobs,
        std::filesystem::path root);

    /// @brief
    /// Purpose: True once initialize_assets has stood the subsystem up (root set, watcher
    /// engaged) — the readiness check that makes initialize_assets a no-op on a second call.
    TBX_DLL_EXPORT bool is_assets_ready(const AssetsState& state);

    /// @brief
    /// Purpose: Number of resident (decoded) assets — debug/tooling.
    TBX_DLL_EXPORT size get_loaded_asset_count(const AssetsState& state);

    /// @brief
    /// Purpose: The asset a handle references, loading it asynchronously when it is not
    /// resident: bytes read + decoded on a worker, stored on the main thread.
    template <typename TAsset>
    Task<Result<std::reference_wrapper<TAsset>>> load(
        AssetsState& state,
        EventsState& events,
        JobsState& jobs,
        AssetHandle<TAsset> handle);

    /// @brief
    /// Purpose: The asset a handle references, decoded inline on the calling thread when it
    /// is not already resident — the renderer/material/startup resolution path.
    template <typename TAsset>
    Result<std::reference_wrapper<TAsset>> load_now(
        AssetsState& state,
        EventsState& events,
        AssetHandle<TAsset> handle);

    /// @brief
    /// Purpose: The resident asset for an id (empty when absent); refreshes its idle timer.
    TBX_DLL_EXPORT std::optional<std::reference_wrapper<std::any>> find_asset(AssetsState& state, const Uuid& id);

    /// @brief
    /// Purpose: Resolves a handle's identity: by id via the meta index, or by path (minting a
    /// .meta sidecar on first sight).
    TBX_DLL_EXPORT Result<ResolvedAssetHandle> resolve_handle(
        AssetsState& state,
        const Uuid& id,
        const std::string& path);

    /// @brief
    /// Purpose: A tracked relative path resolved to its on-disk location (app root first,
    /// engine resources second).
    TBX_DLL_EXPORT std::filesystem::path resolve_asset_path(
        const AssetsState& state,
        const std::string& relative_path);

    /// @brief
    /// Purpose: Stores a decoded asset and announces it (first loads announce too).
    TBX_DLL_EXPORT void store_asset(
        AssetsState& state,
        EventsState& events,
        const Uuid& id,
        const std::string& relative_path,
        std::any asset);

}

#include "tbx/assets/assets.inl"
