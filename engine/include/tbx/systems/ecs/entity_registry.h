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

      private:
        friend class EntityRegistry;

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
    /// Thread Safety: Thread-safe for concurrent registry API calls via internal locking.
    /// Notes: References returned from component access APIs are only safe while callers
    /// externally prevent concurrent mutation of the same entity/component.
    class TBX_API EntityRegistry
    {
      public:
        EntityRegistry();
        ~EntityRegistry() noexcept;

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
