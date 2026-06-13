#pragma once
#include "entt/entt.hpp"
#include "tbx/systems/ecs/entity_registry.generated.h"
#include "tbx/types/components/component.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/uuid.h"
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Represents a lightweight handle to an entity owned by an EntityRegistry.
    /// @details
    /// Ownership: Does not own the registry; caller ensures registry lifetime exceeds this
    /// instance. Thread Safety: Not thread-safe; synchronize external concurrent access.
    [[serializable]];
    [[custom_serialization(serialize, deserialize)]];
    [[printable(
        "Entity{{id={}, name='{}', tag='{}', layer='{}', parent={}}}",
        get_id().value,
        get_name(),
        get_tag(),
        get_layer(),
        get_parent().value)]];
    class TBX_API Entity
    {
      public:
        Entity() = default;
        Entity(const std::string& name, class EntityRegistry& registry);
        Entity(const std::string& name, const Uuid& parent, class EntityRegistry& registry);
        Entity(const Uuid& parent, class EntityRegistry& registry);

      public:
        void destroy();

        Uuid get_id() const;

        std::string get_name() const;
        void set_name(const std::string& name);

        std::string get_tag() const;
        void set_tag(const std::string& tag);

        std::string get_layer() const;
        void set_layer(const std::string& layer);

        Uuid get_parent() const;
        void set_parent(const Uuid& parent);
        bool try_get_parent_entity(Entity& out_parent) const;

        template <typename TComponent>
            requires std::derived_from<TComponent, Component>
        TComponent& add_component(const TComponent& component);

        template <typename TComponent, typename... TArgs>
            requires std::derived_from<TComponent, Component>
        TComponent& add_component(TArgs&&... args);

        template <typename TComponent>
            requires std::derived_from<TComponent, Component>
        void remove_component();

        template <typename... TComponent>
            requires(std::derived_from<TComponent, Component> && ...)
        decltype(auto) get_components() const;

        template <typename TComponent>
            requires std::derived_from<TComponent, Component>
        TComponent& get_component() const;

        template <typename TComponent>
            requires std::derived_from<TComponent, Component>
        bool has_component() const;

      public:
        static std::string serialize(const Entity& entity);

        /// @brief Serializes the entity for the editor: like serialize, but each property is enriched
        /// with its reflection metadata (category/description/view/readonly/hidden, value-type icons)
        /// so the inspector can render it. Persisted files use the lean serialize form instead.
        static std::string describe(const Entity& entity);

        static bool deserialize(std::string_view data, Entity& entity);
        static bool deserialize(
            std::string_view data,
            class EntityRegistry& registry,
            Entity& entity);

        /// @brief Replaces a single component on this entity from its serialized JSON value, reporting a
        /// failure (rather than throwing) when the component is unknown or the JSON cannot be applied.
        static Result apply_component_json(
            const Entity& entity,
            std::string_view component_name,
            std::string_view value_json);

        /// @brief Reads one reflected property of one component as a self-describing
        /// { "type", "value" } node. Fails when the component or property is unknown. The value is the
        /// property's lean serialized form (no editor metadata); use describe for metadata.
        static Result get_component_property(
            const Entity& entity,
            std::string_view component_name,
            std::string_view property_name,
            std::string& out_node_json);

        /// @brief Writes one reflected property of one component in place from its bare serialized value
        /// (not a { "type", "value" } wrapper). Fails when the component or property is unknown or the
        /// value cannot be applied; other properties on the component are left untouched.
        static Result set_component_property(
            const Entity& entity,
            std::string_view component_name,
            std::string_view property_name,
            std::string_view value_json);

        /// @brief Reports whether one reflected property of one component currently equals the value it
        /// has on a default-constructed component. Fails when the component or property is unknown.
        static Result is_component_property_default(
            const Entity& entity,
            std::string_view component_name,
            std::string_view property_name,
            bool& out_is_default);

        /// @brief Resets one reflected property of one component to the value it has on a
        /// default-constructed component. Fails when the component or property is unknown, or the
        /// property has no captured default (its owner is not default-constructible).
        static Result reset_component_property(
            const Entity& entity,
            std::string_view component_name,
            std::string_view property_name);

      private:
        friend class EntityRegistry;

        // Resolves (entity, component_name) to a live component instance under the appropriate registry
        // lock, then runs action; the readable form takes a shared lock and a const instance, the
        // writable form a unique lock and a mutable instance. Centralizes the bind/find/lock/storage
        // boilerplate shared by the reflection property endpoints.
        static Result with_readable_component(
            const Entity& entity,
            std::string_view component_name,
            const std::function<Result(const EntityComponentTypeRegistration&, const void*)>& action);
        static Result with_writable_component(
            const Entity& entity,
            std::string_view component_name,
            const std::function<Result(const EntityComponentTypeRegistration&, void*)>& action);

        std::shared_ptr<class EntityRegistry> _owned_registry = nullptr;
        std::optional<std::reference_wrapper<class EntityRegistry>> _registry = std::nullopt;
        Uuid _id = {};
    };

    /// @brief
    /// Purpose: RAII wrapper that destroys the wrapped entity on scope exit.
    /// @details
    /// Ownership: Owns the contained Entity value only; does not own registry state.
    /// Thread Safety: Not thread-safe; synchronize external concurrent access.
    class TBX_API EntityScope
    {
      public:
        EntityScope(Entity& source);
        ~EntityScope() noexcept;

        Entity entity;
    };

    /// @brief
    /// Purpose: Owns the ECS registry backend and provides entity lifecycle operations.
    /// @details
    /// Ownership: Owns the underlying entt registry instance.
    /// Thread Safety: Each individual call is internally synchronized (a shared_mutex guards the
    /// backing registry), so concurrent calls will not corrupt registry state. It is NOT
    /// transactionally safe: the lock is released when a call returns, so a read-then-write sequence
    /// (e.g. get_name + set_name) can interleave with other writers, and any reference returned by a
    /// component-access API is only valid until the next mutation of that storage — callers that
    /// hold such a reference must externally prevent concurrent mutation of the same
    /// entity/component for the duration they use it.
    ///
    /// Serialization: A registry is itself a serializable value (an array of entity records), so any
    /// asset can hold one as a [[prop]] and have its entities (de)serialized automatically. This is how
    /// WorldChunk / WorldGlobals persist their entities — they own a registry rather than a loose entity
    /// list, so loading populates a registry directly instead of round-tripping each entity.
    [[serializable]];
    [[custom_serialization(serialize, deserialize)]];
    class TBX_API EntityRegistry
    {
      public:
        EntityRegistry();
        ~EntityRegistry() noexcept;

        // Deep-copyable (entities and their components are duplicated). Copyability is required because
        // EntityRegistry is a serializable [[prop]] of WorldChunk / WorldGlobals and the serialization
        // default-fallback path assigns the field. Copies are rare (an empty default fallback, or asset
        // duplication); the live world owns its registry by value and never copies it.
        EntityRegistry(const EntityRegistry& other);
        EntityRegistry& operator=(const EntityRegistry& other);

        bool is_empty() const;
        void clear();

        /// @brief
        /// Purpose: Checks whether this registry currently owns an entity for the specified id.
        /// @details
        /// Ownership: Does not transfer ownership; inspects registry-owned entity state only.
        /// Thread Safety: Thread-safe.
        bool has(const Uuid& id) const;

        template <typename TComponent>
            requires std::derived_from<TComponent, Component>
        bool has(const Uuid& id) const;

        template <typename TComponent, typename... TArgs>
            requires std::derived_from<TComponent, Component>
        TComponent& add(const Uuid& id, TArgs&&... args);
        Uuid add(
            const std::string& name = "",
            const std::string& tag = "",
            const std::string& layer = "",
            const Uuid& parent = Uuid());
        Uuid add(
            const Uuid& id,
            const std::string& name = "",
            const std::string& tag = "",
            const std::string& layer = "",
            const Uuid& parent = Uuid());

        template <typename TComponent>
            requires std::derived_from<TComponent, Component>
        void remove(const Uuid& id);
        void remove(Entity& entity);

        template <typename... TComponent>
            requires(std::derived_from<TComponent, Component> && ...)
        decltype(auto) get_with(const Uuid& id) const;
        template <typename... TComponent>
            requires(std::derived_from<TComponent, Component> && ...)
        std::vector<Entity> get_with() const;
        template <typename... TComponent>
            requires(std::derived_from<TComponent, Component> && ...)
        Entity first_with() const;
        std::vector<Entity> get_all() const;
        Entity get(const Uuid& id) const;

        template <typename... TComponent>
            requires(std::derived_from<TComponent, Component> && ...)
        void for_each_with(const std::function<void(Entity&)>& callback);
        void for_each(const std::function<void(Entity&)>& callback);

        /// @brief Copies one entity — its identity metadata and every registered component — from its
        /// owning registry into this one, by value. Does nothing if the source is invalid or already
        /// belongs to this registry. Used to move loaded chunk/globals entities into the live world
        /// registry without serializing through JSON.
        void absorb(const Entity& source);

      public:
        /// @brief Serializes a whole registry to a JSON array of entity records (each the lean
        /// Entity::serialize form). Custom-serialization entry point for the [[serializable]] registry.
        static std::string serialize(const EntityRegistry& registry);

        /// @brief Populates a registry from the array produced by serialize, adding each record directly
        /// into the registry. Returns false only when the input is not valid JSON.
        static bool deserialize(std::string_view data, EntityRegistry& registry);

      private:
        friend class Entity;

        std::string get_name(const Uuid& id) const;
        void set_name(const Uuid& id, const std::string& name);

        std::string get_tag(const Uuid& id) const;
        void set_tag(const Uuid& id, const std::string& tag);

        Uuid get_parent_id(const Uuid& id) const;
        void set_parent_id(const Uuid& id, const Uuid& parent);

        std::string get_layer(const Uuid& id) const;
        void set_layer(const Uuid& id, const std::string& layer);

        mutable std::shared_mutex _mutex = {};
        std::unique_ptr<entt::registry> _registry = nullptr;
    };
}

#include "tbx/systems/ecs/entity.inl"
#include "tbx/systems/ecs/registry.inl"
