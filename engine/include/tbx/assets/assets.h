#pragma once
#include "tbx/assets/asset_handle.h"
#include "tbx/assets/load.h"
#include "tbx/jobs/jobs.h"
#include "tbx/utils/api.h"
#include "tbx/utils/result.h"
#include "tbx/utils/typedefs.h"
#include "tbx/utils/uuid.h"
#include <any>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

// Async-first asset loading over the static per-type decoder table (tbx::load<T>), with
// .meta identity sidecars and watcher-driven hot reload. Paths resolve against the app's
// asset root first, then the engine's resources folder (TBX_RESOURCES_PATH) — engine-shipped
// models/textures/shaders are ordinary assets too. Module state is created on first use and
// torn down by reset(). Thread safety: the identity and storage maps are mutex-guarded;
// load() decodes on a worker and stores on the main thread, load_now() decodes inline on the
// calling thread.
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
    /// Purpose: Unloads assets that have not been referenced (loaded) for longer than the
    /// idle lifetime, announcing each via the asset_unloaded signal. tbx::run() calls this
    /// every frame; it self-throttles.
    TBX_API void collect_garbage();

    /// @brief
    /// Purpose: Number of resident (decoded) assets — debug/tooling.
    TBX_API size get_loaded_count();

    /// @brief
    /// Purpose: The asset a handle references, loading it asynchronously when it is not
    /// resident: bytes read + decoded on a worker, stored on the main thread.
    template <typename TAsset>
    Task<Result<std::reference_wrapper<TAsset>>> load(AssetHandle<TAsset> handle);

    /// @brief
    /// Purpose: The asset a handle references, decoded inline on the calling thread when it
    /// is not already resident — the renderer/material/startup resolution path.
    template <typename TAsset>
    Result<std::reference_wrapper<TAsset>> load_now(AssetHandle<TAsset> handle);

    /// @brief
    /// Purpose: Unloads everything and stops the watcher; the next set_root() starts fresh.
    /// run() calls this at shutdown.
    TBX_API void reset();

    /// @brief
    /// Purpose: How long an unreferenced asset stays resident before collect_garbage()
    /// unloads it (default 60 seconds).
    TBX_API void set_idle_lifetime(float seconds);

    /// @brief
    /// Purpose: Sets the asset root and starts watching it for hot reload.
    TBX_API void set_root(std::filesystem::path root);

    // Boundary internals the load templates run on — the state itself stays in the .cpp.

    /// @brief
    /// Purpose: The resident asset for an id (nullptr when absent); refreshes its idle timer.
    TBX_API std::any* find_resident_any(const Uuid& id);

    /// @brief
    /// Purpose: Resolves a handle's identity: by id via the meta index, or by path (minting a
    /// .meta sidecar on first sight).
    TBX_API Result<ResolvedHandle> resolve_handle(const Uuid& id, const std::string& path);

    /// @brief
    /// Purpose: A tracked relative path resolved to its on-disk location (app root first,
    /// engine resources second).
    TBX_API std::filesystem::path resolve_path(const std::string& relative_path);

    /// @brief
    /// Purpose: Stores a decoded asset and announces it (first loads announce too).
    TBX_API void store(const Uuid& id, const std::string& relative_path, std::any asset);
}

#include "tbx/assets/assets.inl"
