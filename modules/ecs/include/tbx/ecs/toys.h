#pragma once
#include "tbx/common/collections.h"
#include "tbx/common/string.h"
#include "tbx/common/uuid.h"
#include "tbx/ecs/entity.h"
#include "tbx/ecs/registry.h"

namespace tbx
{
    // Description data for an entity.
    // Ownership: value type; callers own any copies created from this struct.
    // Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    struct ToyDescription
    {
        String name = "";
        String tag = "";
        String layer = "";
        Uuid parent = invalid::uuid;
    };

    // A easy to use game entity wrapper around an entity handle and a registry.
    // Ownership: value type; callers own any copies created from this class. Does not own the
    // underlying registry or entity; but can destroy the entity from the registry. Ensure the
    // registry outlives any toys created from it.
    // synchronize access when sharing instances.
    class Toy
    {
      public:
        Toy() = default;
        Toy(EcsRegistry& reg, const EntityHandle& handle)
            : _registry(&reg)
            , _handle(handle)
        {
        }

        Toy(EcsRegistry& reg, const ToyDescription& desc)
            : _registry(&reg)
            , _handle(reg.create())
        {
            _registry->emplace<ToyDescription>(_handle, desc);
        }

        void destroy()
        {
            _registry->destroy(_handle);
        }

        Uuid get_id() const
        {
            return static_cast<uint32>(_handle);
        }

        ToyDescription& get_description() const
        {
            return _registry->get<ToyDescription>(_handle);
        }

        EcsRegistry& get_registry() const
        {
            return *_registry;
        }

        template <typename T>
        T& add_block(const T& b)
        {
            return _registry->emplace<T>(_handle, b);
        }

        template <typename T, typename... Args>
        T& add_block(Args&&... args)
        {
            return _registry->emplace<T>(_handle, std::forward<Args>(args)...);
        }

        template <typename T>
        void remove_block()
        {
            _registry->remove<T>(_handle);
        }

        template <typename T>
        T& get_block() const
        {
            return _registry->get<T>(_handle);
        }

        template <typename... Block>
        auto get_blocks_of_type() const
        {
            return _registry->view<Block...>();
        }

        operator String() const
        {
            const auto& desc = get_description();
            return String("Toy(ID: ") + String(static_cast<uint>(_handle)) + ", Name: " + desc.name
                   + ", Tag: " + desc.tag + ", Layer: " + desc.layer + ")";
        }

      private:
        EcsRegistry* _registry;
        EntityHandle _handle;
    };

    // A RAII scope for a toy entity.
    // Ownership: value type; callers own any copies created from this class. Owns the underlying
    // toy and destroys it on scope exit. Ensure the registry outlives any toys created from it.
    // Thread Safety: not inherently thread-safe;
    class ToyScope
    {
      public:
        ToyScope(Toy& t)
            : toy(t)
        {
        }
        ~ToyScope()
        {
            toy.destroy();
        }

        Toy toy;
    };

    // A stage containing a registry of entities.
    // Ownership: value type; callers own any copies created from this class. Owns the underlying
    // registry and its entities. Ensure this outlives any toys created from it.
    // Thread Safety: not inherently thread-safe; synchronize access when sharing instances.
    class Stage
    {
      public:
        Stage(String name)
            : _name(name)
            , _id(Uuid::generate())
            , _registry()
        {
        }

        Uuid get_id() const
        {
            return _id;
        }

        String get_name() const
        {
            return _name;
        }

        EcsRegistry& get_registry()
        {
            return _registry;
        }

        const EcsRegistry& get_registry() const
        {
            return _registry;
        }

        operator String() const
        {
            return String("Stage(ID: ") + String(get_id().value) + ", Name: " + get_name() + ")";
        }

        Toy add_toy(
            const String& name,
            const String& tag = "",
            const String& layer = "",
            const Uuid& parent = invalid::uuid)
        {
            ToyDescription desc = {};
            desc.name = name;
            desc.tag = tag;
            desc.layer = layer;
            desc.parent = parent;
            return Toy(_registry, desc);
        }

        Toy add_toy(const ToyDescription& desc)
        {
            return Toy(_registry, desc);
        }

        Toy get_toy(const Uuid& id)
        {
            return Toy(_registry, static_cast<EntityHandle>(id.value));
        }

        List<Toy> view_all_toys()
        {
            List<Toy> toys = {};
            auto view = _registry.view<ToyDescription>();
            for (auto entity : view)
                toys.emplace(_registry, entity);
            return toys;
        }

        template <typename... Block>
        List<Toy> view_with_type()
        {
            List<Toy> toys = {};
            auto view = _registry.view<Block...>();
            for (auto entity : view)
                toys.emplace(_registry, entity);
            return toys;
        }

      private:
        String _name = "";
        Uuid _id = invalid::uuid;
        EcsRegistry _registry = {};
    };

}
