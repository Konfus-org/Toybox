#pragma once
#include "entt/entt.hpp"
#include "tbx/common/uuid.h"
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace tbx
{
    /// <summary>
    /// Purpose: Strongly typed identifier for a registry entity entry.
    /// </summary>
    /// <remarks>
    /// Ownership: Value type with no ownership semantics.
    /// Thread Safety: Safe to copy across threads; mutating the referenced entity is not
    /// thread-safe.
    /// </remarks>
    using EntityHandle = entt::entity;

    class EntityRegistry;

    /// <summary>
    /// Purpose: Represents a lightweight handle to an entity owned by an EntityRegistry.
    /// </summary>
    /// <remarks>
    /// Ownership: Does not own the registry; caller ensures registry lifetime exceeds this
    /// instance. Thread Safety: Not thread-safe; synchronize external concurrent access.
    /// </remarks>
    class TBX_API Entity
    {
      public:
        Entity() = default;

        /// <summary>
        /// Purpose: Creates and registers a new entity with an optional explicit name.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not own the provided registry reference.
        /// Thread Safety: Not thread-safe; synchronize external concurrent access.
        /// </remarks>
        Entity(const std::string& name, EntityRegistry& registry, const Uuid& parent = Uuid::NONE);

        /// <summary>
        /// Purpose: Creates and registers a new entity with an auto-generated fallback name.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not own the provided registry reference.
        /// Thread Safety: Not thread-safe; synchronize external concurrent access.
        /// </remarks>
        Entity(EntityRegistry& registry, const Uuid& parent = Uuid::NONE);

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

        template <typename... TComponents>
        Entity& add_components(TComponents&&... components)
        {
            (add_component<std::decay_t<TComponents>>(std::forward<TComponents>(components)), ...);
            return *this;
        }

        template <typename TComponent>
        TComponent& add_component(const TComponent& component)
        {
            return _registry->_pool->emplace<TComponent>(_handle, component);
        }

        template <typename TComponent, typename... TArgs>
        TComponent& add_component(TArgs&&... args)
        {
            return _registry->_pool->emplace<TComponent>(_handle, std::forward<TArgs>(args)...);
        }

        template <typename TComponent>
        void remove_component()
        {
            _registry->_pool->remove<TComponent>(_handle);
        }

        template <typename TComponent>
        TComponent& get_component() const
        {
            return _registry->_pool->get<TComponent>(_handle);
        }

        template <typename... TComponent>
        auto get_components() const
        {
            return _registry->_pool->view<TComponent...>();
        }

        template <typename TComponent>
        bool has_component() const
        {
            return _registry->_pool->all_of<TComponent>(_handle);
        }

      private:
        friend class EntityRegistry;

        Entity(EntityRegistry& registry, const EntityHandle& handle);

        EntityRegistry* _registry = nullptr;
        EntityHandle _handle = entt::null;
    };

    /// <summary>
    /// Purpose: Formats an entity identifier and metadata for debugging output.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns an owned std::string.
    /// Thread Safety: Safe for concurrent use when the entity metadata is not being mutated.
    /// </remarks>
    TBX_API std::string to_string(const Entity& entity);

    /// <summary>
    /// Purpose: RAII wrapper that destroys the wrapped entity on scope exit.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the contained Entity value only; does not own registry state.
    /// Thread Safety: Not thread-safe; synchronize external concurrent access.
    /// </remarks>
    class TBX_API EntityScope
    {
      public:
        EntityScope(Entity& t);
        ~EntityScope() noexcept;

        Entity entity;
    };

    /// <summary>
    /// Purpose: Owns the ECS registry backend and provides entity lifecycle operations.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the underlying entt registry instance.
    /// Thread Safety: Not thread-safe; synchronize external concurrent access.
    /// </remarks>
    class TBX_API EntityRegistry
    {
      public:
        EntityRegistry();
        ~EntityRegistry() noexcept;

        void destroy(Entity& entity);
        void destroy_all();
        bool is_empty() const;

        EntityHandle register(
            const std::string& name = "",
            const std::string& tag = "",
            const std::string& layer = "",
            const Uuid& parent = invalid::uuid);

        void unregister(const EntityHandle& handle);
        void unregister(Entity& entity);

        Entity get(const Uuid& id);

        std::vector<Entity> get_all();

        template <typename... TBlocks>
        std::vector<Entity> get_with()
        {
            std::vector<Entity> entities = {};
            auto view = _pool->view<TBlocks...>();
            for (auto entity : view)
                entities.emplace_back(*this, entity);
            return entities;
        }

        void for_each(const std::function<void(Entity&)>& callback);

        template <typename... TBlocks>
        void for_each_with(const std::function<void(Entity&)>& callback)
        {
            auto view = _pool->view<TBlocks...>();
            for (auto entity_handle : view)
            {
                Entity entity(*this, entity_handle);
                callback(entity);
            }
        }

      private:
        friend class Entity;

        std::string get_entity_name(const EntityHandle& handle) const;
        void set_entity_name(const EntityHandle& handle, const std::string& name);

        std::string get_entity_tag(const EntityHandle& handle) const;
        void set_entity_tag(const EntityHandle& handle, const std::string& tag);

        std::string get_entity_layer(const EntityHandle& handle) const;
        void set_entity_layer(const EntityHandle& handle, const std::string& layer);

        Uuid get_entity_parent(const EntityHandle& handle) const;
        void set_entity_parent(const EntityHandle& handle, const Uuid& parent);

        using EntityPool = entt::registry;
        std::unique_ptr<EntityPool> _pool = nullptr;
    };
}
