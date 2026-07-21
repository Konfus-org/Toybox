#pragma once
#include "entt/entt.hpp"
#include "tbx/systems/assets/serialization.h"
#include "tbx/tbx_api.h"
#include "tbx/types/uuid.h"
#include <concepts>
#include <functional>
#include <string>
#include <string_view>
#include <typeindex>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Provides identity metadata common to public ECS components.
    /// @details
    /// Ownership: Value type inherited by component payloads. Copies preserve identity.
    /// Thread Safety: Safe to copy between threads; synchronize shared mutation externally.
    struct TBX_API Component
    {
        Uuid id = Uuid::generate();

        /// @brief
        /// Purpose: Whether this component is active. Disabled components are skipped by their systems.
        /// @details
        /// Ownership: Value type. Hidden from the property grid — the inspector exposes it as the toggle in
        /// the component header rather than as an ordinary row.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        bool is_enabled = true;
    };

    /// @brief
    /// Purpose: Bridges serializable component value types into Entity custom serialization.
    struct EntityComponentTypeRegistration
    {
        std::string name = {};
        std::string type_name = {};
        std::type_index type = std::type_index(typeid(void));
        entt::id_type type_id = {};
        std::function<std::string(const void*)> write_value = {};
        std::function<bool(std::string_view, entt::registry&, entt::entity)> read_value = {};
        // Copies a live component value from one registry into another without going through JSON, so an
        // EntityRegistry can absorb entities from another registry directly (see EntityRegistry::absorb).
        std::function<bool(const void*, entt::registry&, entt::entity)> copy_value = {};
        // Destroys every instance of this component type in an entt registry (running each component's
        // destructor). Invoked when the type's owning plugin unloads: the type becomes unavailable, so
        // its components are stripped from all live registries while the module is still mapped. Like
        // copy_value, it is statically typed in the component's own defining module.
        std::function<void(entt::registry&)> clear_all = {};
    };

    TBX_API std::vector<EntityComponentTypeRegistration> get_entity_component_type_registrations();
    TBX_API void register_entity_component_type_entry(
        RuntimeRegistrations& owner,
        EntityComponentTypeRegistration entry);

    template <typename TComponent>
        requires std::derived_from<TComponent, Component>
    static bool register_entity_component_type(
        RuntimeRegistrations& owner,
        const SerializableTypeRegistration& registration)
    {
        const auto read_value = registration.read_value;
        register_entity_component_type_entry(
            owner,
            EntityComponentTypeRegistration {
                .name = registration.name,
                .type_name = registration.type_name,
                .type = registration.type,
                .type_id = entt::type_hash<TComponent>::value(),
                .write_value = registration.write_value,
                .read_value =
                    [read_value](
                        std::string_view data,
                        entt::registry& registry,
                        entt::entity entity)
                {
                    try
                    {
                        auto component = TComponent {};
                        if (!read_value(data, &component))
                            return false;

                        registry.emplace_or_replace<TComponent>(entity, component);
                        return true;
                    }
                    catch (...)
                    {
                        return false;
                    }
                },
                .copy_value =
                    [](const void* source, entt::registry& registry, entt::entity entity)
                {
                    try
                    {
                        registry.emplace_or_replace<TComponent>(
                            entity,
                            *static_cast<const TComponent*>(source));
                        return true;
                    }
                    catch (...)
                    {
                        return false;
                    }
                },
                .clear_all =
                    [](entt::registry& registry)
                {
                    registry.clear<TComponent>();
                },
            });
        return true;
    }

    template <typename TComponent>
        requires std::derived_from<TComponent, Component>
    struct SerializableTypeRegistrationHook<TComponent>
    {
        static bool register_type(
            RuntimeRegistrations& owner,
            const SerializableTypeRegistration& registration)
        {
            return register_entity_component_type<TComponent>(owner, registration);
        }
    };
}
