#pragma once
#include "asset_ops.h"
#include "world_manager.h"
#include "tbx/systems/files/json.h"
#include "tbx/utils/result.h"
#include <functional>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: Resolves the editor's uniform sync.* { path } addressing (see EngineAddress on the
    /// editor side) and routes each verb (describe/set/reset/isDefault) to the implementation that
    /// owns what the path names — entity/component/world ops on the WorldManager, asset describes on
    /// AssetOps. Owns the path grammar; the targets own the behavior.
    /// @details
    /// Ownership: Holds non-owning references to the world manager and asset ops it routes into.
    /// Thread Safety: Main-thread only (request handling).
    class SyncPathRouter
    {
      public:
        SyncPathRouter(WorldManager& world, AssetOps& assets);

      public:
        /// @brief Reads the object addressed by { path } as its describe body — the editor's sync.describe.
        /// The path names an object (an entity, a component, …); see EngineAddress on the editor side.
        Result sync_describe_path(const tbx::Json& params, tbx::Json& out_reply) const;

        /// @brief Writes the field addressed by { path } to { value } — the editor's sync.set.
        Result sync_set_path(const tbx::Json& params) const;

        /// @brief Resets the field addressed by { path } to its default — the editor's sync.reset.
        Result sync_reset_path(const tbx::Json& params) const;

        /// @brief Whether the field addressed by { path } currently holds its default — sync.isDefault.
        Result sync_is_default_path(const tbx::Json& params, bool& out_is_default) const;

      private:
        // The component-property implementations the path verbs route into, addressed by the legacy
        // { entityId, worldAssetId, component, property[, value] } params the path resolves to.
        Result sync_set(const tbx::Json& params) const;
        Result sync_reset(const tbx::Json& params) const;
        Result sync_is_default(const tbx::Json& params, bool& out_is_default) const;

      private:
        std::reference_wrapper<WorldManager> _world;
        std::reference_wrapper<AssetOps> _assets;
    };
}
