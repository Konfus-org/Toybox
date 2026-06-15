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
    // storage. Everything below is built on them plus the reflection registry — which already carries
    // each property's attributes and defaults — so the per-property editing surface needs nothing
    // beyond serialization.

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

    /// @brief Reads one property of one component as a self-describing { "type", "value" } node.
    /// Fails when the component or property is unknown.
    TBX_API Result serialize_component_property(
        const Entity& entity,
        std::string_view component_name,
        std::string_view property_name,
        std::string& out_node_json);

    /// @brief Writes one property of one component in place from its bare serialized value (not a
    /// { "type", "value" } wrapper); other properties on the component are left untouched. Fails when
    /// the component or property is unknown or the value cannot be applied.
    TBX_API Result apply_component_property(
        const Entity& entity,
        std::string_view component_name,
        std::string_view property_name,
        std::string_view value_json);

    /// @brief Reports whether one property currently equals the value it has on a default-constructed
    /// component. Fails when the component or property is unknown.
    TBX_API Result is_component_property_default(
        const Entity& entity,
        std::string_view component_name,
        std::string_view property_name,
        bool& out_is_default);

    /// @brief Resets one property to the value it has on a default-constructed component. Fails when
    /// the component or property is unknown, or the property has no captured default (its owner is not
    /// default-constructible).
    TBX_API Result reset_component_property(
        const Entity& entity,
        std::string_view component_name,
        std::string_view property_name);
}
