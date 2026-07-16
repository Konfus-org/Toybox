#pragma once
#include "tbx/systems/files/json.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/result.h"
#include <string_view>

namespace tbx::studio_bridge
{
    struct EngineServices;

    // The editor's asset-catalog queries and edits over RPC — listing/describing registered assets,
    // saving edited bodies, creating default-valued assets (with their `.meta` identity sidecars),
    // forgetting deleted ones, minting fresh ids, and the browser's preview stats. Pure
    // asset-registry work: they never touch live world state (the world-facing ops live in
    // world_ops). Main-thread only (request handling).

    // --- Describe / list (read) ---

    /// @brief Describes an { assetId }'s editable properties for the asset inspector.
    Result describe_asset(
        const EngineServices& services, const tbx::Json& params, tbx::Json& out_reply);

    /// @brief Lists every registered asset plus the script catalog (for the asset/script pickers).
    tbx::Json list_assets(const EngineServices& services);

    /// @brief Reports a model { assetId }'s real-world stats for the browser's hover HUD: its bounding-box
    /// size (width/height/depth in engine units/metres), triangle count, material-slot count, and mesh
    /// count. Returns an empty object for a non-model asset.
    tbx::Json asset_preview_stats(const EngineServices& services, const tbx::Json& params);

    /// @brief Writes a `<asset>.meta` sidecar ({ id, version }) for every registered project asset that
    /// lacks one, persisting its (until-now in-memory) id so references stay stable across runs. Skips
    /// self-describing script metas and build-output copies. Replies { generated: N }.
    tbx::Json generate_missing_metas(const EngineServices& services);

    // --- Persistence ---

    /// @brief Saves an edited asset ({ assetId } + values) to disk.
    Result save_asset(const EngineServices& services, const tbx::Json& params);

    /// @brief Applies one live per-field edit to a loaded asset (a material tweaked in the Asset Viewer,
    /// the project's AppSettings) — mutating the resident instance in memory so a previewed material
    /// re-renders and per-frame settings self-apply. It never writes the file; persistence stays with
    /// save_asset. A no-op-safe round-trip through the type's own serialize/deserialize, so untouched
    /// fields are preserved. Reached through the uniform sync.set path (asset/{id}/{property}); the
    /// property and value come pre-parsed from the address by the sync path resolver.
    Result apply_asset_property(
        const EngineServices& services, uint64 asset_id, std::string_view property, const tbx::Json& value);

    /// @brief Creates a new, default-valued asset of the registered { type } at the project-relative
    /// { path }, writing its body plus a fresh-id `<path>.meta` sidecar so it is discoverable and its
    /// references stay stable. A { type } of "World" scaffolds the linked `.world` + `.globals` + `.chunk`
    /// trio. Replies { id, path }.
    Result create_asset(
        const EngineServices& services, const tbx::Json& params, tbx::Json& out_reply);

    /// @brief Drops an asset from the in-memory registry by { id, path }. The editor deletes the file(s)
    /// itself, then calls this so a delete is reflected immediately without waiting on the file watcher.
    Result forget_asset(const EngineServices& services, const tbx::Json& params);

    /// @brief Mints a fresh, engine-unique asset id (so the editor can author a sidecar — e.g. a new
    /// script's `.h.meta` — without risking an id collision). Replies { id }.
    Result new_asset_id(tbx::Json& out_reply);

    /// @brief Force-registers an existing asset file by { path } into the registry (loading it establishes
    /// its id→path mapping), so a later id-based reference to it resolves. The type is dispatched by
    /// extension. Used by the editor to register the assets it wants — e.g. a preview world's bundled
    /// dependencies whose directory the registry's source scan skips. Replies { id }.
    Result load_asset(const EngineServices& services, const tbx::Json& params, tbx::Json& out_reply);
}
