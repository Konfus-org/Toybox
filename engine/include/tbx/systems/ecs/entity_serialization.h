#pragma once
#include "tbx/systems/assets/serialization.h"
#include "tbx/tbx_api.h"
#include <string>
#include <string_view>

namespace tbx
{
    class Entity;

    // Whole-component serialization primitives. These bridge a live component on an entity to and
    // from its serialized JSON and are the only operations here that touch the registry's component
    // storage. The per-property helpers below are built on them: a property is read/written by
    // round-tripping the whole component through its own serialize/deserialize.

    /// @brief Serializes one component of an entity to its full { "type", "value" } JSON (every field
    /// present). Fails when the entity/registry is gone or the component is unknown or missing.
    TBX_API Result serialize_component(
        const Entity& entity,
        std::string_view component_name,
        std::string& out_json);

    /// @brief Replaces one component on an entity from its serialized JSON value. Fails when the
    /// component is unknown or the JSON cannot be applied.
    TBX_API Result apply_component(
        const Entity& entity,
        std::string_view component_name,
        std::string_view value_json);

    /// @brief Adds the named component to an entity at its default values. Fails when the component is
    /// unknown or the entity already has it.
    TBX_API Result add_default_component(const Entity& entity, std::string_view component_name);

    /// @brief Removes the named component from an entity. Fails when the component is unknown or the
    /// entity does not have it.
    TBX_API Result remove_component(const Entity& entity, std::string_view component_name);

    /// @brief Reads one property of one component as its bare serialized value. Fails when the
    /// component or property is unknown.
    TBX_API Result serialize_component_property(
        const Entity& entity,
        std::string_view component_name,
        std::string_view property_name,
        std::string& out_value_json);

    /// @brief Writes one property of one component in place from its bare serialized value; other
    /// properties on the component are left untouched. Fails when the component or property is unknown
    /// or the value cannot be applied.
    TBX_API Result apply_component_property(
        const Entity& entity,
        std::string_view component_name,
        std::string_view property_name,
        std::string_view value_json);
}
