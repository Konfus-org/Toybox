#pragma once
#include "entt/entt.hpp"
#include "tbx/types/uuid.h"
#include "tbx/types/components/component.h"
#include <concepts>
#include <shared_mutex>

namespace tbx
{
    class Entity;

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
        friend struct Serializer<Entity>;

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

#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/registry.inl"
