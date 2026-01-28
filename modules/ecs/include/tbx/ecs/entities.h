#pragma once
#include "tbx/common/int.h"
#include "tbx/common/uuid.h"
#include "tbx/ecs/registry.h"
#include <string>

namespace tbx
{
    // An entity identifier.
    // Ownership: value type; callers own any copies created from this alias.
    // Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using EntityHandle = entt::entity;

    // Description data for an entity.
    // Ownership: value type; callers own any copies created from this struct.
    // Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    struct TBX_API EntityDescription
    {
        std::string name = "";
        std::string tag = "";
        std::string layer = "";
        Uuid parent = invalid::uuid;
    };

    // A easy to use game entity wrapper around an entity handle and a registry.
    // Ownership: value type; callers own any copies created from this class. Does not own the
    // underlying registry or entity; but can destroy the entity from the registry. Ensure the
    // registry outlives any toys created from it.
    // synchronize access when sharing instances.
    class TBX_API Entity
    {
      public:
        Entity() = default;
        Entity(EntityRegistry* reg, const EntityHandle& handle);
        Entity(EntityRegistry* reg, const EntityDescription& desc);

        void destroy();

        Uuid get_id() const;

        EntityDescription& get_description() const;

        template <typename TComponent>
        TComponent& add_component(const TComponent& b)
        {
            return _registry->emplace<TComponent>(_handle, b);
        }

        template <typename TComponent, typename... TArgs>
        TComponent& add_component(TArgs&&... args)
        {
            return _registry->emplace<TComponent>(_handle, std::forward<TArgs>(args)...);
        }

        template <typename TComponent>
        void remove_component()
        {
            _registry->remove<TComponent>(_handle);
        }

        template <typename TComponent>
        TComponent& get_component() const
        {
            return _registry->get<TComponent>(_handle);
        }

        template <typename... TComponent>
        auto get_components() const
        {
            return _registry->view<TComponent...>();
        }

        template <typename TComponent>
        bool has_component() const
        {
            return _registry->all_of<TComponent>(_handle);
        }

      private:
        EntityRegistry* _registry;
        EntityHandle _handle;
    };

    /// <summary>Purpose: Formats an entity identifier and description for debugging
    /// output.</summary> <remarks>Ownership: Returns an owned std::string. Thread Safety: Safe for
    /// concurrent use when the entity is not mutated.</remarks>
    TBX_API std::string to_string(const Entity& entity);

    // A RAII scope for an entity.
    // Ownership: value type; callers own any copies created from this class. Owns the underlying
    // toy and destroys it on scope exit. Ensure the registry outlives any toys created from it.
    // Thread Safety: not inherently thread-safe;
    class TBX_API EntityScope
    {
      public:
        EntityScope(Entity& t);
        ~EntityScope() noexcept;

        Entity entity;
    };

    // Manages entities.
    // Ownership: value type; callers own any copies created from this class. Owns the underlying
    // registry and its entities. Ensure this outlives any entities created from it.
    // Thread Safety: not inherently thread-safe; synchronize access when sharing instances.
    class TBX_API ECS
    {
      public:
        ECS();
        ~ECS() noexcept;

        void destroy_all_entities();
        bool is_empty();

        Entity create_entity(
            const std::string& name,
            const std::string& tag = "",
            const std::string& layer = "",
            const Uuid& parent = invalid::uuid);

        Entity create_entity(const EntityDescription& desc);

        Entity get_entity(const Uuid& id);

        std::vector<Entity> get_all_entities();

        template <typename... TBlocks>
        std::vector<Entity> get_entities_with()
        {
            std::vector<Entity> toys = {};
            auto view = _registry->view<TBlocks...>();
            for (auto entity : view)
                toys.emplace_back(_registry.get(), entity);
            return toys;
        }

      private:
        std::unique_ptr<EntityRegistry> _registry = nullptr;
    };

    namespace invalid
    {
        inline constexpr EntityHandle entity_handle = entt::null;
    }
}
