#pragma once
#include "engine_services.h"
#include "tbx/systems/files/json.h"
#include "tbx/types/uuid.h"
#include "tbx/utils/result.h"
#include <filesystem>
#include <functional>
#include <string>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: Serves the editor's asset-catalog queries and edits over RPC — listing/describing
    /// registered assets, saving edited bodies, creating default-valued assets (with their `.meta`
    /// identity sidecars), forgetting deleted ones, minting fresh ids, and the browser's preview
    /// stats. Pure asset-registry work: it never touches live world state (the world-facing ops stay
    /// on WorldManager).
    /// @details
    /// Ownership: Holds a non-owning reference to the engine services (to reach the asset manager,
    /// its serialization registry, and the scripting registry). Thread Safety: Main-thread only
    /// (request handling).
    class AssetOps
    {
      public:
        explicit AssetOps(EngineServices& services);

      public:
        // --- Describe / list (read) ---

        /// @brief Describes an { assetId }'s editable properties for the asset inspector.
        Result describe_asset(const tbx::Json& params, tbx::Json& out_reply) const;

        /// @brief Lists every registered asset plus the script catalog (for the asset/script pickers).
        tbx::Json list_assets() const;

        /// @brief Reports a model { assetId }'s real-world stats for the browser's hover HUD: its bounding-box
        /// size (width/height/depth in engine units/metres), triangle count, material-slot count, and mesh
        /// count. Returns an empty object for a non-model asset.
        tbx::Json asset_preview_stats(const tbx::Json& params) const;

        /// @brief Writes a `<asset>.meta` sidecar ({ id, version }) for every registered project asset that
        /// lacks one, persisting its (until-now in-memory) id so references stay stable across runs. Skips
        /// self-describing script metas and build-output copies. Replies { generated: N }.
        tbx::Json generate_missing_metas() const;

        // --- Persistence ---

        /// @brief Saves an edited asset ({ assetId } + values) to disk.
        Result save_asset(const tbx::Json& params) const;

        /// @brief Creates a new, default-valued asset of the registered { type } at the project-relative
        /// { path }, writing its body plus a fresh-id `<path>.meta` sidecar so it is discoverable and its
        /// references stay stable. A { type } of "World" scaffolds the linked `.world` + `.globals` + `.chunk`
        /// trio. Replies { id, path }.
        Result create_asset(const tbx::Json& params, tbx::Json& out_reply) const;

        /// @brief Drops an asset from the in-memory registry by { id, path }. The editor deletes the file(s)
        /// itself, then calls this so a delete is reflected immediately without waiting on the file watcher.
        Result forget_asset(const tbx::Json& params) const;

        /// @brief Mints a fresh, engine-unique asset id (so the editor can author a sidecar — e.g. a new
        /// script's `.h.meta` — without risking an id collision). Replies { id }.
        Result new_asset_id(tbx::Json& out_reply) const;

      private:
        // Writes a default-constructed asset of the registered `type` to `resolved` (its body) plus a
        // fresh-id `<resolved>.meta` sidecar, returning the minted id in `out_id`. The shared core of
        // create_asset and the world-scaffold helper. Fails when the type is unknown/non-serializable,
        // something already exists at the path, or the file write fails.
        Result write_default_asset(
            const std::string& type,
            const std::filesystem::path& resolved,
            tbx::Uuid& out_id,
            const tbx::Json* initial_body = nullptr) const;

        // Scaffolds a usable, empty world at `resolved` (a `.world`): a fresh `.globals` and `.chunk`
        // beside it, then the `.world` that links them. Returns the `.world`'s minted id in `out_id`.
        Result write_default_world(const std::filesystem::path& resolved, tbx::Uuid& out_id) const;

      private:
        std::reference_wrapper<EngineServices> _services;
    };
}
