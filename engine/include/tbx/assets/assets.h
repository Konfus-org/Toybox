#pragma once
#include "tbx/assets/asset.h"
#include "tbx/assets/asset_handle.h"
#include "tbx/assets/load.h"
#include "tbx/events/events.h"
#include "tbx/files/watcher.h"
#include "tbx/jobs/jobs.h"
#include "tbx/utils/api.h"
#include "tbx/utils/result.h"
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

// Async-first asset loading over the static per-type decoder table (tbx::load<T>), with
// .meta identity sidecars and watcher-driven hot reload. Paths resolve against the app's
// asset root first, then the engine's resources folder (TBX_RESOURCES_PATH) — engine-shipped
// models/textures/shaders are ordinary assets too. The state is runtime.assets, dropped
// (watcher included) when the RuntimeState dies.
// Thread safety: the identity and storage maps are mutex-guarded; load() decodes on a worker
// and stores on the main thread, load_now() decodes inline on the calling thread.
namespace tbx::assets
{
    /// @brief
    /// Purpose: A handle after identity resolution: the id plus the path to decode from.
    struct TBX_API ResolvedHandle
    {
        Uuid id = {};
        std::string relative_path = {};
    };

    /// @brief
    /// Purpose: One resident (decoded) asset plus its bookkeeping.
    struct TBX_API LoadedAsset
    {
        std::any data;
        std::chrono::steady_clock::time_point last_access = {};
    };

    /// @brief
    /// Purpose: The assets module's state, held by value on the Runtime — two maps and the
    /// glue around them: `assets` is the identity index (relative path -> uuid, filled from
    /// .meta sidecars) and `loaded_assets` is resident storage. The mutex guards both maps
    /// (loads run on workers); everything dies with the runtime, watcher included.
    struct TBX_API AssetsState
    {
        std::filesystem::path root = {};
        mutable std::mutex mutex;
        std::unordered_map<std::string, Uuid> assets;
        std::unordered_map<Uuid, LoadedAsset> loaded_assets;
        float idle_lifetime_seconds = 60.0f;
        std::chrono::steady_clock::time_point last_purge = std::chrono::steady_clock::now();
        std::optional<files::FileWatcher> watcher;
    };

    /// @brief
    /// Purpose: Sets the asset root and starts watching it for hot reload (change reports
    /// marshal through the jobs queue onto the main thread).
    TBX_API void set_root(
        AssetsState& state,
        events::EventsState& events,
        jobs::JobsState& jobs,
        std::filesystem::path root);

    /// @brief
    /// Purpose: Unloads assets that have not been referenced (loaded) for longer than
    /// state.idle_lifetime_seconds, announcing each via the asset_unloaded signal. tbx::run()
    /// calls this every frame; it self-throttles.
    TBX_API void update(AssetsState& state, events::EventsState& events);

    /// @brief
    /// Purpose: Number of resident (decoded) assets — debug/tooling.
    TBX_API size get_loaded_count(const AssetsState& state);

    /// @brief
    /// Purpose: The asset a handle references, loading it asynchronously when it is not
    /// resident: bytes read + decoded on a worker, stored on the main thread.
    template <typename TAsset>
    jobs::Task<Result<std::reference_wrapper<TAsset>>> load(
        AssetsState& state,
        events::EventsState& events,
        jobs::JobsState& jobs,
        AssetHandle<TAsset> handle);

    /// @brief
    /// Purpose: The asset a handle references, decoded inline on the calling thread when it
    /// is not already resident — the renderer/material/startup resolution path.
    template <typename TAsset>
    Result<std::reference_wrapper<TAsset>> load_now(
        AssetsState& state,
        events::EventsState& events,
        AssetHandle<TAsset> handle);

    /// @brief
    /// Purpose: The resident asset for an id (empty when absent); refreshes its idle timer.
    TBX_API std::optional<std::reference_wrapper<std::any>> find(
        AssetsState& state,
        const Uuid& id);

    /// @brief
    /// Purpose: Resolves a handle's identity: by id via the meta index, or by path (minting a
    /// .meta sidecar on first sight).
    TBX_API Result<ResolvedHandle> resolve_handle(
        AssetsState& state,
        const Uuid& id,
        const std::string& path);

    /// @brief
    /// Purpose: A tracked relative path resolved to its on-disk location (app root first,
    /// engine resources second).
    TBX_API std::filesystem::path resolve_path(
        const AssetsState& state,
        const std::string& relative_path);

    /// @brief
    /// Purpose: Stores a decoded asset and announces it (first loads announce too).
    TBX_API void store(
        AssetsState& state,
        events::EventsState& events,
        const Uuid& id,
        const std::string& relative_path,
        std::any asset);
}

#include "tbx/assets/assets.inl"
