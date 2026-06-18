#pragma once
#include "entt/entt.hpp"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/registry.generated.h"
#include "tbx/types/components/component.h"
#include "tbx/types/uuid.h"
#include <concepts>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Owns the ECS registry backend and provides entity lifecycle operations.
    /// @details
    /// Ownership: Owns the underlying entt registry instance.
    /// Thread Safety: Each individual call is internally synchronized (a shared_mutex guards the
    /// backing registry), so concurrent calls will not corrupt registry state. It is NOT
    /// transactionally safe: the lock is released when a call returns, so a read-then-write
    /// sequence (e.g. get_name + set_name) can interleave with other writers, and any reference
    /// returned by a component-access API is only valid until the next mutation of that storage —
    /// callers that hold such a reference must externally prevent concurrent mutation of the same
    /// entity/component for the duration they use it.
    ///
    /// Serialization: A registry is itself a serializable value (an array of entity records), so
    /// any asset can hold one as a [[prop]] and have its entities (de)serialized automatically.
    /// This is how WorldChunk / WorldGlobals persist their entities — they own a registry rather
    /// than a loose entity list, so loading populates a registry directly instead of round-tripping
    /// each entity.
    [[serializable]];
    [[custom_serialization(serialize, deserialize)]];
    class TBX_API EntityRegistry
    {
      public:
        EntityRegistry();
        ~EntityRegistry() noexcept;

        // Deep-copyable (entities and their components are duplicated). Copyability is required
        // because EntityRegistry is a serializable [[prop]] of WorldChunk / WorldGlobals and the
        // serialization default-fallback path assigns the field. Copies are rare (an empty default
        // fallback, or asset duplication); the live world owns its registry by value and never
        // copies it.
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

        /// @brief Copies one entity — its identity metadata and every registered component — from
        /// its owning registry into this one, by value. Does nothing if the source is invalid or
        /// already belongs to this registry. Used to move loaded chunk/globals entities into the
        /// live world registry without serializing through JSON.
        void absorb(const Entity& source);

      public:
        /// @brief Serializes a whole registry to a JSON array of entity records (each the lean
        /// Entity::serialize form). Custom-serialization entry point for the [[serializable]]
        /// registry.
        static std::string serialize(const EntityRegistry& registry);

        /// @brief Populates a registry from the array produced by serialize, adding each record
        /// directly into the registry. Returns false only when the input is not valid JSON.
        static bool deserialize(std::string_view data, EntityRegistry& registry);

      private:
        friend class Entity;
        // The component-level serialization primitives reach into the component storage below.
        friend TBX_API Result serialize_component(
            const Entity& entity,
            std::string_view component_name,
            std::string& out_json);
        friend TBX_API Result apply_component(
            const Entity& entity,
            std::string_view component_name,
            std::string_view value_json);

        std::string get_name(const Uuid& id) const;
        void set_name(const Uuid& id, const std::string& name);

        std::string get_tag(const Uuid& id) const;
        void set_tag(const Uuid& id, const std::string& tag);

        Uuid get_parent_id(const Uuid& id) const;
        void set_parent_id(const Uuid& id, const Uuid& parent);

        int get_order_value(const Uuid& id) const;
        void set_order_value(const Uuid& id, int order);

        bool get_enabled(const Uuid& id) const;
        void set_enabled(const Uuid& id, bool enabled);

        // Enabled check for an already-resolved handle, assuming the caller already holds _mutex.
        // Lets the templated queries filter disabled entities under a single lock instead of
        // re-locking through get_enabled per entity. Absence of the flag means enabled.
        bool is_handle_enabled(entt::entity handle) const;

        std::string get_layer(const Uuid& id) const;
        void set_layer(const Uuid& id, const std::string& layer);

        mutable std::shared_mutex _mutex = {};
        std::unique_ptr<entt::registry> _registry = nullptr;
    };
}

// Both classes are now complete, so the template bodies (which each need the other type) can be
// pulled in. These live here rather than in entity.h because entity.h is included before
// EntityRegistry is defined.
#include "tbx/systems/ecs/entity.inl"
#include "tbx/systems/ecs/registry.inl"
