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
    /// Purpose: Serves the editor's world/entity/asset queries and edits over RPC — describing the
    /// world, entities, assets and the settings schema; per-property reflection (get/set/reset/
    /// is-default); component edits; entity create/destroy/move/rename/global/enable; and world/asset
    /// saves.
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

        /// @brief Describes an { assetId }'s editable properties for the asset inspector.
        Result describe_asset(const tbx::Json& params, tbx::Json& out_reply) const;

        /// @brief The application settings schema for the settings editor.
        tbx::Json describe_settings() const;

        /// @brief Lists every registered asset plus the script catalog (for the asset/script pickers).
        tbx::Json list_assets() const;

        /// @brief Lists the addable component types (for the inspector's add-component menu).
        tbx::Json list_component_types() const;

        /// @brief Returns a model's hard material slots ({name, id} per slot) so the Renderer inspector
        /// can size its slot list and auto-fill each slot's default material from the model.
        Result model_slots(const tbx::Json& params, tbx::Json& out_reply) const;

        /// @brief Ensures an in-memory MaterialInstance that shows the given { textureId } on the bundled
        /// unlit preview material, replying with its id. The asset-preview editor sets a Renderer slot to it
        /// to show a texture on a primitive (a texture isn't itself a material, and the editor can't register
        /// in-memory assets, so the bridge vends this preview material). Reused (deduplicated) per texture.
        Result preview_texture_material(const tbx::Json& params, tbx::Json& out_reply) const;

        // --- Per-property reflection ---

        /// @brief Reads one property node ({ entityId, component, path }) for the property grid.
        Result reflect_get(const tbx::Json& params, tbx::Json& out_node) const;

        /// @brief Writes one property to the value in params.
        Result reflect_set(const tbx::Json& params) const;

        /// @brief Resets one property to its default.
        Result reflect_reset(const tbx::Json& params) const;

        /// @brief Whether one property currently holds its default value.
        Result reflect_is_default(const tbx::Json& params, bool& out_is_default) const;

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

        /// @brief Renames an entity.
        Result set_entity_name(const tbx::Json& params) const;

        /// @brief Toggles whether an entity is global (persists across world chunks).
        Result set_entity_global(const tbx::Json& params) const;

        /// @brief Toggles an entity's enabled flag.
        Result set_entity_enabled(const tbx::Json& params) const;

        // --- Persistence ---

        /// @brief Saves the active world to disk.
        Result save_world() const;

        /// @brief Saves an edited asset ({ assetId } + values) to disk.
        Result save_asset(const tbx::Json& params) const;

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
        // Resolves and validates an entityId param against the active world, falling back to any
        // asset-preview world so the inspector can describe/edit a previewed asset's entity.
        Result resolve_reflect_entity(const tbx::Json& params, tbx::Entity& out_entity) const;

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
