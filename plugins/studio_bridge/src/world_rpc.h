#pragma once
#include "engine_services.h"
#include "tbx/systems/ecs/registry.h"
#include "tbx/systems/files/json.h"
#include "tbx/utils/result.h"
#include <unordered_map>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: Serves the editor's world/entity/asset queries and edits over RPC — describing the
    /// world, entities, assets and the settings schema; per-property reflection (get/set/reset/
    /// is-default); component edits; entity create/destroy/move/rename/global/enable; and world/asset
    /// saves.
    /// @details
    /// Ownership: Stateless; borrows the engine services. Thread Safety: Main-thread only (request
    /// handling).
    class WorldRpc
    {
      public:
        explicit WorldRpc(EngineServices& services);

        // Describe / list (read).
        tbx::Json describe_world() const;
        Result describe_entity(const tbx::Json& params, tbx::Json& out_reply) const;
        Result describe_asset(const tbx::Json& params, tbx::Json& out_reply) const;
        tbx::Json describe_settings() const;
        tbx::Json list_assets() const;
        tbx::Json list_component_types() const;

        // Per-property reflection.
        Result reflect_get(const tbx::Json& params, tbx::Json& out_node) const;
        Result reflect_set(const tbx::Json& params) const;
        Result reflect_reset(const tbx::Json& params) const;
        Result reflect_is_default(const tbx::Json& params, bool& out_is_default) const;

        // Component + entity edits.
        Result apply_component(const tbx::Json& params) const;
        Result add_component(const tbx::Json& params) const;
        Result remove_component(const tbx::Json& params) const;
        Result add_script(const tbx::Json& params) const;
        Result create_entity(const tbx::Json& params, tbx::Json& out_reply) const;
        Result destroy_entity(const tbx::Json& params) const;
        Result move_entity(const tbx::Json& params) const;
        Result set_entity_name(const tbx::Json& params) const;
        Result set_entity_global(const tbx::Json& params) const;
        Result set_entity_enabled(const tbx::Json& params) const;

        // Persistence.
        Result save_world() const;
        Result save_asset(const tbx::Json& params) const;

      private:
        // The component-type icon side table shared by describe_world and describe_entity.
        tbx::Json component_type_icons() const;
        // Resolves and validates an entityId param against the active world.
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

        EngineServices& _services;
    };
}
