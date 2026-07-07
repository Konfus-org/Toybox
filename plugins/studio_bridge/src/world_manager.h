#pragma once
#include "engine_services.h"
#include "view_manager.h"
#include "tbx/systems/ecs/registry.h"
#include "tbx/systems/files/json.h"
#include "tbx/utils/result.h"
#include <functional>
#include <unordered_map>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: Serves the editor's world/entity queries and edits over RPC — describing the
    /// world, entities and the settings schema; component edits; entity create/destroy/move/
    /// rename/global/enable; and world open/save. (The asset-catalog ops live on AssetOps and the
    /// sync.* path addressing on SyncPathRouter, which routes into the ops here.)
    /// @details
    /// Ownership: Holds non-owning references to the engine services and the view manager (to
    /// reach asset-preview worlds). Thread Safety: Main-thread only (request handling).
    class WorldManager
    {
      public:
        WorldManager(EngineServices& services, ViewManager& views);

      public:
        // --- Describe / list (read) ---

        /// @brief Snapshots a world (its entities + components) for the editor's world tree. Targets the
        /// world named by an optional { worldId } param (an asset-preview world); defaults to the active
        /// editing world.
        tbx::Json describe_world(const tbx::Json& params) const;

        /// @brief Describes one { entityId } (its components + values) for the inspector.
        Result describe_entity(const tbx::Json& params, tbx::Json& out_reply) const;

        /// @brief The application settings schema for the settings editor.
        tbx::Json describe_settings() const;

        /// @brief The sync catalog: every registered component type by wire name with its icon badge (the
        /// engine-truth source the editor's add-component menu and typed-component reconciliation draw from).
        tbx::Json sync_catalog() const;

        /// @brief Returns a model's hard material slots ({name, id} per slot) so the Renderer inspector
        /// can size its slot list and auto-fill each slot's default material from the model.
        Result model_slots(const tbx::Json& params, tbx::Json& out_reply) const;

        /// @brief Ensures an in-memory MaterialInstance that shows the given { textureId } on the bundled
        /// unlit preview material, replying with its id. The asset-preview editor sets a Renderer slot to it
        /// to show a texture on a primitive (a texture isn't itself a material, and the editor can't register
        /// in-memory assets, so the bridge vends this preview material). Reused (deduplicated) per texture.
        Result preview_texture_material(const tbx::Json& params, tbx::Json& out_reply) const;

        // --- Component + entity edits ---

        /// @brief Replaces a component's whole serialized state on an entity.
        Result apply_component(const tbx::Json& params) const;

        /// @brief Adds a component of the named type to an entity.
        Result add_component(const tbx::Json& params) const;

        /// @brief Removes a component from an entity.
        Result remove_component(const tbx::Json& params) const;

        /// @brief Binds a script to an entity.
        Result add_script(const tbx::Json& params) const;

        /// @brief Creates an entity, replying with its new id.
        Result create_entity(const tbx::Json& params, tbx::Json& out_reply) const;

        /// @brief Destroys an entity (and its descendants).
        Result destroy_entity(const tbx::Json& params) const;

        /// @brief Reparents and/or reorders an entity in the world tree.
        Result move_entity(const tbx::Json& params) const;

        // --- Sync-router entry points (the minimal surface SyncPathRouter routes into) ---

        /// @brief Resolves and validates an entityId param against the active world, falling back to any
        /// asset-preview world so the inspector can describe/edit a previewed asset's entity.
        Result resolve_sync_entity(const tbx::Json& params, tbx::Entity& out_entity) const;

        /// @brief Routes a sync.set whose path resolved to an entity scalar field (name/is_enabled/
        /// is_global/tags) to the matching set_entity_* op, placing `value` under the key that op reads.
        /// `params` already carries the resolved entityId/worldAssetId.
        Result set_entity_scalar(tbx::Json& params, const std::string& field, const tbx::Json& value) const;

        // --- Persistence ---

        /// @brief Saves the active world to disk.
        Result save_world() const;

        /// @brief Opens a world/chunk asset (by id) as the active editing world, replacing the current
        /// one.
        Result open_world(const tbx::Json& params) const;

      private:
        // The world a world-level op targets: the asset-preview world named by an optional { worldId }
        // param, else the active editing world. Used by create/describe (which carry a worldId).
        std::shared_ptr<tbx::World> world_for(const tbx::Json& params) const;

        // The world that owns the entity named by { entityId }: the active world when it holds the id,
        // else the asset-preview world that does. Lets per-entity structural ops (set/add/remove
        // component, move/destroy/rename/global/enable) edit a previewed asset's entity, not just the
        // active world's. Null when no world holds the id.
        std::shared_ptr<tbx::World> owning_world(const tbx::Json& params) const;

        // The component-type icon side table shared by describe_world and describe_entity.
        tbx::Json component_type_icons() const;

        // Validates the { entityId } param and resolves the world that owns it (via owning_world),
        // returning the id alongside — the shared preamble of every per-entity op.
        Result resolve_entity_world(
            const tbx::Json& params,
            std::shared_ptr<tbx::World>& out_world,
            tbx::Uuid& out_id) const;

        // The entity scalar-field implementations sync.set's path verb routes into (via
        // set_entity_scalar), addressed by the resolved { entityId, worldAssetId } params.

        // Renames an entity.
        Result set_entity_name(const tbx::Json& params) const;
        // Toggles whether an entity is global (persists across world chunks).
        Result set_entity_global(const tbx::Json& params) const;
        // Toggles an entity's enabled flag.
        Result set_entity_enabled(const tbx::Json& params) const;
        // Replaces an entity's persistent (serialized) gameplay tags with the provided set; runtime-only
        // tags (e.g. editor.selected) are left untouched.
        Result set_entity_tags(const tbx::Json& params) const;

        // Expands each bound script's overrides into the script's FULL editable field set so the inspector
        // can show (and edit) every property of a script — not just the ones already set away from default.
        // Each emitted field carries its type token + [[tbx::asset]] choices (for the right widget/filter),
        // its current value (the override if set, else the script default), an is_default flag, and the lean
        // default value (so the editor can reset and persist only the fields actually changed). The cache
        // reuses one (lean, attributed) schema pair per script type across the entities of a describe pass.
        void enrich_script_overrides(
            tbx::Json& entity_json,
            std::unordered_map<uint64, std::pair<tbx::Json, tbx::Json>>& schema_cache) const;
        // The field schema of the script asset with the given id: lean ({ type, value=default }) when
        // attributed is false, attribute-enriched (type token + baked [[tbx::asset]] choices) when true.
        // Empty object when the id resolves to no describable script.
        tbx::Json describe_script_schema(uint64 script_id, bool attributed) const;

      private:
        std::reference_wrapper<EngineServices> _services;
        std::reference_wrapper<ViewManager> _views;
    };
}
