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
    struct EntityDescription
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
    class Entity
    {
      public:
        Entity() = default;
        Entity(EntityRegistry& reg, const EntityHandle& handle)
            : _registry(&reg)
            , _handle(handle)
        {
        }

        Entity(EntityRegistry& reg, const EntityDescription& desc)
            : _registry(&reg)
            , _handle(reg.create())
        {
            _registry->emplace<EntityDescription>(_handle, desc);
        }

        void destroy()
        {
            _registry->destroy(_handle);
        }

        Uuid get_id() const
        {
            return static_cast<uint32>(_handle);
        }

        EntityDescription& get_description() const
        {
            return _registry->get<EntityDescription>(_handle);
        }

        EntityRegistry& get_registry() const
        {
            return *_registry;
        }

        template <typename T>
        T& add_component(const T& b)
        {
            return _registry->emplace<T>(_handle, b);
        }

        template <typename T, typename... Args>
        T& add_component(Args&&... args)
        {
            return _registry->emplace<T>(_handle, std::forward<Args>(args)...);
        }

        template <typename T>
        void remove_component()
        {
            _registry->remove<T>(_handle);
        }

        template <typename T>
        T& get_component() const
        {
            return _registry->get<T>(_handle);
        }

        template <typename... Block>
        auto get_components() const
        {
            return _registry->view<Block...>();
        }

      private:
        EntityRegistry* _registry;
        EntityHandle _handle;
    };

    /// <summary>Purpose: Formats an entity identifier and description for debugging output.</summary>
    /// <remarks>Ownership: Returns an owned std::string. Thread Safety: Safe for concurrent use when the entity is not mutated.</remarks>
    TBX_API std::string to_string(const Entity& entity);

    // A RAII scope for an entity.
    // Ownership: value type; callers own any copies created from this class. Owns the underlying
    // toy and destroys it on scope exit. Ensure the registry outlives any toys created from it.
    // Thread Safety: not inherently thread-safe;
    class EntityScope
    {
      public:
        EntityScope(Entity& t)
            : entity(t)
        {
        }
        ~EntityScope()
        {
            entity.destroy();
        }

        Entity entity;
    };

    // Manages entities.
    // Ownership: value type; callers own any copies created from this class. Owns the underlying
    // registry and its entities. Ensure this outlives any entities created from it.
    // Thread Safety: not inherently thread-safe; synchronize access when sharing instances.
    class EntityDirector
    {
      public:
        EntityRegistry& get_registry()
        {
            return _registry;
        }

        const EntityRegistry& get_registry() const
        {
            return _registry;
        }

        Entity create_entity(
            const std::string& name,
            const std::string& tag = "",
            const std::string& layer = "",
            const Uuid& parent = invalid::uuid)
        {
            EntityDescription desc = {};
            desc.name = name;
            desc.tag = tag;
            desc.layer = layer;
            desc.parent = parent;
            return Entity(_registry, desc);
        }

        Entity create_entity(const EntityDescription& desc)
        {
            return Entity(_registry, desc);
        }

        Entity get_entity(const Uuid& id)
        {
            return Entity(_registry, static_cast<EntityHandle>(id.value));
        }

        std::vector<Entity> get_all_entities()
        {
            std::vector<Entity> toys = {};
            auto view = _registry.view<EntityDescription>();
            for (auto entity : view)
                toys.emplace_back(_registry, entity);
            return toys;
        }

        template <typename... Block>
        std::vector<Entity> get_entities()
        {
            std::vector<Entity> toys = {};
            auto view = _registry.view<Block...>();
            for (auto entity : view)
                toys.emplace_back(_registry, entity);
            return toys;
        }

      private:
        EntityRegistry _registry = {};
    };

    namespace invalid
    {
        inline constexpr EntityHandle entity_handle = entt::null;
    }
}
