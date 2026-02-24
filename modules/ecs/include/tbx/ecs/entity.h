#pragma once
#include "tbx/common/uuid.h"
#include "tbx/math/transform.h"
#include <string>
#include <utility>

namespace tbx
{
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
        Entity(const std::string& name, EntityRegistry* registry);
        Entity(const std::string& name, EntityRegistry& registry);
        Entity(const std::string& name, const Uuid& parent, EntityRegistry* registry);
        Entity(const std::string& name, const Uuid& parent, EntityRegistry& registry);
        Entity(const Uuid& parent, EntityRegistry* registry);
        Entity(const Uuid& parent, EntityRegistry& registry);

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
        /// <summary>
        /// Purpose: Resolves and returns this entity's parent entity handle.
        /// </summary>
        /// <remarks>
        /// Ownership: Writes a non-owning entity handle into out_parent when available.
        /// Thread Safety: Not thread-safe; synchronize external concurrent access.
        /// </remarks>
        bool try_get_parent_entity(Entity& out_parent) const;

        template <typename TComponent>
        TComponent& add_component(const TComponent& component);

        template <typename TComponent, typename... TArgs>
        TComponent& add_component(TArgs&&... args);

        template <typename TComponent>
        void remove_component();

        template <typename... TComponent>
        decltype(auto) get_components() const;

        template <typename TComponent>
        TComponent& get_component() const;

        template <typename TComponent>
        bool has_component() const;

      private:
        friend class EntityRegistry;

        EntityRegistry* _registry = nullptr;
        Uuid _id = {};
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
    /// Purpose: Resolves an entity transform in world space by composing parent local transforms.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns an owned Transform value snapshot.
    /// Thread Safety: Not thread-safe; synchronize external concurrent access.
    /// Notes: Entity Transform components are authored and stored in local space.
    /// </remarks>
    TBX_API Transform get_world_space_transform(const Entity& entity);

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
        EntityScope(Entity& source);
        ~EntityScope() noexcept;

        Entity entity;
    };
}

#include "tbx/ecs/entity.inl"
