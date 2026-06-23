#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/registry.h"

namespace tbx
{
    Entity::Entity(const std::string& name, EntityRegistry& registry)
        : Entity(name, Uuid::NONE, registry)
    {
    }

    Entity::Entity(const Uuid& parent, EntityRegistry& registry)
        : Entity("", parent, registry)
    {
    }

    Entity::Entity(const std::string& name, const Uuid& parent, EntityRegistry& registry)
        : _registry(std::ref(registry))
        , _id(registry.add(name, "", parent))
    {
    }

    void Entity::destroy()
    {
        if (!_registry.has_value())
            return;

        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to destroy a stale entity handle.");
            _id = {};
            _registry = std::nullopt;
            return;
        }

        registry.remove(*this);
    }

    Uuid tbx_reference_id(const Entity& entity)
    {
        // A reference field's stored id; get_id() returns the raw _id when there is no bound registry,
        // which is exactly the state of a reference entity.
        return entity.get_id();
    }

    void tbx_bind_reference(Entity& entity, const Uuid& id)
    {
        // A reference holds only the target id — no registry. The game resolves it against the live world.
        entity._id = id;
    }

    Uuid Entity::get_id() const
    {
        if (!_registry.has_value())
            return _id;

        // Callers commonly use `get_id().is_valid()` as a safe validity probe.
        if (!_registry->get().has(_id))
            return {};

        return _id;
    }

    std::string Entity::get_name() const
    {
        if (!_registry.has_value())
            return "";
        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to read entity name from a stale handle.");
            return "";
        }

        return registry.get_name(_id);
    }

    void Entity::set_name(const std::string& name)
    {
        if (!_registry.has_value())
            return;

        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to write entity name to a stale handle.");
            return;
        }

        registry.set_name(_id, name);
    }

    void Entity::add_tag(const std::string& name, bool serialized)
    {
        if (!_registry.has_value())
            return;
        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to add a tag to a stale entity handle.");
            return;
        }

        registry.add_tag(_id, name, serialized);
    }

    void Entity::remove_tag(const std::string& name)
    {
        if (!_registry.has_value())
            return;
        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to remove a tag from a stale entity handle.");
            return;
        }

        registry.remove_tag(_id, name);
    }

    bool Entity::has_tag(const std::string& query) const
    {
        if (!_registry.has_value())
            return false;
        auto& registry = _registry->get();
        if (!registry.has(_id))
            return false;

        return registry.has_tag(_id, query);
    }

    std::vector<std::string> Entity::get_tags() const
    {
        if (!_registry.has_value())
            return {};
        auto& registry = _registry->get();
        if (!registry.has(_id))
            return {};

        return registry.get_tags(_id);
    }

    std::vector<std::string> Entity::get_persistent_tags() const
    {
        if (!_registry.has_value())
            return {};
        auto& registry = _registry->get();
        if (!registry.has(_id))
            return {};

        return registry.get_persistent_tags(_id);
    }

    std::vector<std::string> Entity::get_runtime_tags() const
    {
        if (!_registry.has_value())
            return {};
        auto& registry = _registry->get();
        if (!registry.has(_id))
            return {};

        return registry.get_runtime_tags(_id);
    }

    std::string Entity::get_layer() const
    {
        if (!_registry.has_value())
            return "";
        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to read entity layer from a stale handle.");
            return "";
        }

        return registry.get_layer(_id);
    }

    void Entity::set_layer(const std::string& layer)
    {
        if (!_registry.has_value())
            return;

        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to write entity layer to a stale handle.");
            return;
        }

        registry.set_layer(_id, layer);
    }

    Uuid Entity::get_parent() const
    {
        if (!_registry.has_value())
            return {};
        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to read entity parent from a stale handle.");
            return {};
        }

        return registry.get_parent_id(_id);
    }

    void Entity::set_parent(const Uuid& parent)
    {
        if (!_registry.has_value())
            return;

        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to write entity parent to a stale handle.");
            return;
        }

        registry.set_parent_id(_id, parent);
    }

    int Entity::get_order() const
    {
        if (!_registry.has_value())
            return 0;
        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to read entity order from a stale handle.");
            return 0;
        }

        return registry.get_order_value(_id);
    }

    void Entity::set_order(int order)
    {
        if (!_registry.has_value())
            return;

        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to write entity order to a stale handle.");
            return;
        }

        registry.set_order_value(_id, order);
    }

    bool Entity::is_enabled() const
    {
        if (!_registry.has_value())
            return true;
        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to read entity enabled flag from a stale handle.");
            return true;
        }

        return registry.get_enabled(_id);
    }

    void Entity::set_enabled(bool enabled)
    {
        if (!_registry.has_value())
            return;

        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to write entity enabled flag to a stale handle.");
            return;
        }

        registry.set_enabled(_id, enabled);
    }

    bool Entity::try_get_parent_entity(Entity& out_parent) const
    {
        out_parent = Entity {};
        if (!_registry.has_value())
            return false;
        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to resolve parent from a stale entity handle.");
            return false;
        }

        const Uuid parent_id = registry.get_parent_id(_id);
        if (!parent_id.is_valid())
            return false;

        out_parent = registry.get(parent_id);
        return out_parent.get_id().is_valid();
    }

    Transform Transform::to_world_space(const Entity& entity) const
    {
        auto world_transform = *this;

        auto cursor = entity;
        auto parent = Entity {};
        size_t iteration_count = 0U;
        static constexpr size_t MAX_PARENT_DEPTH = 1024U;
        while (cursor.try_get_parent_entity(parent) && iteration_count < MAX_PARENT_DEPTH)
        {
            if (parent.get_id() == cursor.get_id())
                break;

            if (parent.has_component<Transform>())
            {
                const auto& parent_transform = parent.get_component<Transform>();
                world_transform = compose_world_space_transform(parent_transform, world_transform);
            }

            cursor = parent;
            ++iteration_count;
        }

        return world_transform;
    }

    EntityScope::EntityScope(Entity& source)
        : entity(source)
    {
    }

    EntityScope::~EntityScope() noexcept
    {
        entity.destroy();
    }
}
