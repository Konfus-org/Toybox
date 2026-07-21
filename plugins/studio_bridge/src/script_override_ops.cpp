#include "script_override_ops.h"
#include "bridge_utils.h"
#include "engine_services.h"
#include "wire.h"
#include "world_ops.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/scripting/script_system.h"
#include "tbx/types/components/script_container.h"
#include <string>

namespace tbx::studio_bridge
{
    // Resolves the { entityId, bindingId } params to the entity and its script binding. The binding
    // pointer stays valid for the request: bindings live on the entity's component and nothing else
    // mutates the world mid-request (main-thread request handling).
    static Result resolve_binding(
        const EngineServices& services,
        ViewState& views,
        const tbx::Json& params,
        tbx::Entity& out_entity,
        tbx::ScriptContainerBinding*& out_binding)
    {
        if (const auto resolved = resolve_sync_entity(services, views, params, out_entity);
            !resolved)
            return resolved;

        auto binding_id = uint64(0);
        if (const auto required = require_uint(params, Wire::BINDING_ID, binding_id); !required)
            return required;

        if (!out_entity.has_component<tbx::ScriptContainer>())
            return Result(false, "Entity has no script container.");

        auto& container = out_entity.get_component<tbx::ScriptContainer>();
        for (auto& binding : container.scripts)
        {
            if (binding.binding_id.value == binding_id)
            {
                out_binding = &binding;
                return Result::OK;
            }
        }
        return Result(false, "Entity has no script binding with that id.");
    }

    // The bare default value for one field of the binding's script, or an error naming what's missing.
    // Nested member tails (a '/'-joined property) are not overridable — overrides are flat per-field, so
    // the schema lookup fails cleanly for them.
    static Result find_schema_field(
        const EngineServices& services,
        const tbx::ScriptContainerBinding& binding,
        const std::string& property,
        tbx::Json& out_field)
    {
        const auto schema = describe_script_schema(services, binding.script.id.value);
        if (!schema.is_object() || schema.empty())
            return Result(false, "The binding's script has no describable schema.");

        const auto field_iterator = schema.find(property);
        if (field_iterator == schema.end())
            return Result(false, "The script has no overridable field '" + property + "'.");

        // The plain schema field's value IS the script's default for this field (a bare value).
        out_field = *field_iterator;
        return Result::OK;
    }

    // Lands one field's effective value on the binding's LIVE instance, if it has one this session.
    // A failure here never fails the edit — the stored override is already correct and re-applies on
    // the next instantiate — so a live-apply miss (a reloading script, a backend without live apply)
    // downgrades to a warning.
    static void apply_to_live_instance(
        const EngineServices& services,
        const tbx::Entity& entity,
        const tbx::ScriptContainerBinding& binding,
        const std::string& property,
        const tbx::Json& field_value)
    {
        auto script_system = services.script_system.lock();
        auto active_world = services.active_world();
        if (!script_system || !active_world)
            return;

        auto overrides = tbx::Json::object();
        overrides[property] = field_value;
        const auto applied = script_system->apply_overrides(
            tbx::ScriptLookup {
                .world = active_world->id,
                .entity = entity.get_id(),
                .script = binding.script.id,
                .binding_id = binding.binding_id,
            },
            overrides);
        if (!applied)
        {
            TBX_TRACE_WARNING(
                "Stored script override '{}' but could not apply it to the live instance: {}",
                property,
                applied.get_report());
        }
    }

    Result set_script_override(
        const EngineServices& services, ViewState& views, const tbx::Json& params)
    {
        auto property = std::string();
        if (const auto required = require_string(params, Wire::PROPERTY, property); !required)
            return required;

        const auto value_iterator = params.find(Wire::VALUE);
        if (value_iterator == params.end())
            return Result(false, "Missing 'value'.");

        auto entity = tbx::Entity();
        tbx::ScriptContainerBinding* binding = nullptr;
        if (const auto resolved = resolve_binding(services, views, params, entity, binding);
            !resolved)
            return resolved;

        auto schema_field = tbx::Json();
        if (const auto found = find_schema_field(services, *binding, property, schema_field);
            !found)
            return found;

        // Store the incoming bare value — exactly the plain per-field shape the generated
        // apply/instantiate readers consume. A value equal to the script's default is not an override
        // at all: erase it, so the persisted blob stays lean and the field keeps tracking future
        // changes to the script's source default.
        if (schema_field == *value_iterator)
            binding->overrides.erase(property);
        else
            binding->overrides[property] = *value_iterator;

        apply_to_live_instance(services, entity, *binding, property, *value_iterator);
        return Result::OK;
    }
}
