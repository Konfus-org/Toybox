#pragma once
#include "engine_services.h"
#include "tbx/systems/ecs/registry.h"
#include "tbx/systems/files/json.h"
#include "tbx/utils/result.h"

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

        // Per-property reflection.
        Result reflect_get(const tbx::Json& params, tbx::Json& out_node) const;
        Result reflect_set(const tbx::Json& params) const;
        Result reflect_reset(const tbx::Json& params) const;
        Result reflect_is_default(const tbx::Json& params, bool& out_is_default) const;

        // Component + entity edits.
        Result apply_component(const tbx::Json& params) const;
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

        EngineServices& _services;
    };
}
