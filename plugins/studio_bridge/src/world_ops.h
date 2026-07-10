#pragma once
#include "tbx/systems/ecs/registry.h"
#include "tbx/systems/files/json.h"
#include "tbx/utils/result.h"
#include <string>

namespace tbx::studio_bridge
{
    struct EngineServices;
    struct ViewState;

    // The editor's world/entity queries and edits over RPC — describing the world, entities and the
    // settings schema; component edits; entity create/destroy/move/rename/global/enable; and world
    // open/save. (The asset-catalog ops live in asset_ops and the sync.* path addressing in
    // sync_path_ops, which routes into the ops here.) The view state reaches the asset-preview
    // worlds; only the functions that resolve them take it. Main-thread only (request handling).

    // --- Describe / list (read) ---

    /// @brief Snapshots a world (its entities + components) for the editor's world tree. Targets the
    /// world named by an optional { worldId } param (an asset-preview world); defaults to the active
    /// editing world.
    tbx::Json describe_world(
        const EngineServices& services, ViewState& views, const tbx::Json& params);

    /// @brief Describes one { entityId } (its components + values) for the inspector.
    Result describe_entity(
        const EngineServices& services,
        ViewState& views,
        const tbx::Json& params,
        tbx::Json& out_reply);

    /// @brief Ensures an in-memory MaterialInstance that shows the given { textureId } on the bundled
    /// unlit preview material, replying with its id. The asset-preview editor sets a Renderer slot to it
    /// to show a texture on a primitive (a texture isn't itself a material, and the editor can't register
    /// in-memory assets, so the bridge vends this preview material). Reused (deduplicated) per texture.
    Result preview_texture_material(
        const EngineServices& services, const tbx::Json& params, tbx::Json& out_reply);

    // --- Component + entity edits ---

    /// @brief Adds a component of the named type to an entity.
    Result add_component(
        const EngineServices& services, ViewState& views, const tbx::Json& params);

    /// @brief Removes a component from an entity.
    Result remove_component(
        const EngineServices& services, ViewState& views, const tbx::Json& params);

    /// @brief Binds a script to an entity.
    Result add_script(const EngineServices& services, ViewState& views, const tbx::Json& params);

    /// @brief Creates an entity, replying with its new id.
    Result create_entity(
        const EngineServices& services,
        ViewState& views,
        const tbx::Json& params,
        tbx::Json& out_reply);

    /// @brief Destroys an entity (and its descendants).
    Result destroy_entity(
        const EngineServices& services, ViewState& views, const tbx::Json& params);

    /// @brief Reparents and/or reorders an entity in the world tree.
    Result move_entity(const EngineServices& services, ViewState& views, const tbx::Json& params);

    // --- Sync-router entry points (the minimal surface sync_path_ops routes into) ---

    /// @brief Resolves and validates an entityId param against the active world, falling back to any
    /// asset-preview world so the inspector can describe/edit a previewed asset's entity.
    Result resolve_sync_entity(
        const EngineServices& services,
        ViewState& views,
        const tbx::Json& params,
        tbx::Entity& out_entity);

    /// @brief Routes a sync.set whose path resolved to an entity scalar field (name/is_enabled/
    /// is_global/tags) to the matching set_entity_* op, placing `value` under the key that op reads.
    /// `params` already carries the resolved entityId/worldAssetId.
    Result set_entity_scalar(
        const EngineServices& services,
        ViewState& views,
        tbx::Json& params,
        const std::string& field,
        const tbx::Json& value);

    // --- Persistence ---

    /// @brief Saves the active world to disk.
    Result save_world(const EngineServices& services);

    /// @brief Opens a world/chunk asset (by id) as the active editing world, replacing the current
    /// one.
    Result open_world(const EngineServices& services, const tbx::Json& params);
}
