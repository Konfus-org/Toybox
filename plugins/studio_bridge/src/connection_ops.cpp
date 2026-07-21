#include "connection_ops.h"
#include "bridge_utils.h"
#include "engine_services.h"
#include "wire.h"
#include "world_ops.h"
#include "tbx/systems/ecs/entity_serialization.h"
#include "tbx/types/components/property_connections.h"
#include <algorithm>
#include <string>

namespace tbx::studio_bridge
{
    // The property endpoints both verbs read: the target (the shared entityId/component/property
    // keys) resolved to its entity, plus the target's component/property names.
    static Result read_target(
        const EngineServices& services,
        ViewState& views,
        const tbx::Json& params,
        tbx::Entity& out_entity,
        std::string& out_component,
        std::string& out_property)
    {
        if (const auto resolved = resolve_sync_entity(services, views, params, out_entity);
            !resolved)
            return resolved;
        if (const auto required = require_string(params, Wire::COMPONENT, out_component); !required)
            return required;
        return require_string(params, Wire::PROPERTY, out_property);
    }

    // The JSON kind token of one property's serialized (now plain) value, plus whether it is numeric —
    // what the connect-time compatibility check compares. With no type tokens left in the wire format,
    // two properties are compatible when their bare values share a JSON kind (numbers cross int/float).
    static Result property_shape(
        const tbx::Entity& entity,
        std::string_view component,
        std::string_view property,
        std::string& out_type,
        bool& out_numeric)
    {
        auto node_json = std::string();
        if (const auto read = serialize_component_property(entity, component, property, node_json);
            !read)
            return read;

        const auto node = tbx::Json::parse(node_json, nullptr, false);
        if (node.is_discarded())
            return Result(false, "The property did not serialize to a value.");

        out_numeric = node.is_number();
        if (node.is_number())
            out_type = "number";
        else if (node.is_string())
            out_type = "string";
        else if (node.is_boolean())
            out_type = "boolean";
        else if (node.is_array())
            out_type = "array";
        else if (node.is_object())
            out_type = "object";
        else
            out_type = "null";
        return Result::OK;
    }

    Result add_connection(
        const EngineServices& services, ViewState& views, const tbx::Json& params)
    {
        auto target = tbx::Entity();
        auto target_component = std::string();
        auto target_property = std::string();
        if (const auto resolved =
                read_target(services, views, params, target, target_component, target_property);
            !resolved)
            return resolved;

        auto source_id = uint64(0);
        if (const auto required = require_uint(params, Wire::SOURCE_ENTITY_ID, source_id);
            !required)
            return required;
        auto source_component = std::string();
        if (const auto required = require_string(params, Wire::SOURCE_COMPONENT, source_component);
            !required)
            return required;
        auto source_property = std::string();
        if (const auto required = require_string(params, Wire::SOURCE_PROPERTY, source_property);
            !required)
            return required;

        auto world = services.active_world();
        if (!world || !world->has(tbx::Uuid(source_id)))
            return Result(false, "The source entity does not exist.");
        auto source = world->get(tbx::Uuid(source_id));

        // Connect-time compatibility: the two properties' serialized JSON kinds must match, with
        // numbers allowed to cross int/float (the apply converts). No silent lossy coercions beyond
        // that — a mismatch is refused here rather than failing every tick.
        auto source_type = std::string();
        auto source_numeric = false;
        if (const auto shaped =
                property_shape(source, source_component, source_property, source_type, source_numeric);
            !shaped)
            return shaped;
        auto target_type = std::string();
        auto target_numeric = false;
        if (const auto shaped = property_shape(
                target, target_component, target_property, target_type, target_numeric);
            !shaped)
            return shaped;
        if (source_type != target_type && !(source_numeric && target_numeric))
            return Result(
                false,
                "The properties' types don't match ('" + source_type + "' → '" + target_type + "').");

        // One driver per target property: adding over an existing link replaces it.
        auto& connections = target.has_component<tbx::PropertyConnections>()
                                ? target.get_component<tbx::PropertyConnections>()
                                : target.add_component<tbx::PropertyConnections>();
        std::erase_if(
            connections.connections,
            [&](const tbx::PropertyConnection& connection)
            {
                return connection.target_component == target_component
                       && connection.target_field == target_property;
            });
        connections.connections.push_back(tbx::PropertyConnection {
            .source_entity = tbx::Uuid(source_id),
            .source_component = source_component,
            .source_field = source_property,
            .target_component = target_component,
            .target_field = target_property,
        });
        return Result::OK;
    }

    Result remove_connection(
        const EngineServices& services, ViewState& views, const tbx::Json& params)
    {
        auto target = tbx::Entity();
        auto target_component = std::string();
        auto target_property = std::string();
        if (const auto resolved =
                read_target(services, views, params, target, target_component, target_property);
            !resolved)
            return resolved;

        if (!target.has_component<tbx::PropertyConnections>())
            return Result(false, "The entity has no property connections.");

        // The component stays even when its last connection goes: an empty component is valid data
        // and removing it here would surprise an editor that only asked to drop one link.
        auto& connections = target.get_component<tbx::PropertyConnections>();
        const auto erased = std::erase_if(
            connections.connections,
            [&](const tbx::PropertyConnection& connection)
            {
                return connection.target_component == target_component
                       && connection.target_field == target_property;
            });
        return erased > 0 ? Result::OK
                          : Result(false, "No connection drives that property.");
    }
}
