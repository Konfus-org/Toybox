#include "tbx/ecs/entities.h"

namespace tbx
{
    //// Entity class implementation ////

    Entity::Entity(EntityRegistry& reg, const EntityHandle& handle)
        : _registry(&reg)
        , _handle(handle)
    {
    }

    Entity::Entity(EntityRegistry& reg, const EntityDescription& desc)
        : _registry(&reg)
        , _handle(reg.create())
    {
        _registry->emplace<EntityDescription>(_handle, desc);
    }

    void Entity::destroy()
    {
        _registry->destroy(_handle);
    }

    Uuid Entity::get_id() const
    {
        return static_cast<uint32>(_handle);
    }

    EntityDescription& Entity::get_description() const
    {
        return _registry->get<EntityDescription>(_handle);
    }

    //// EntityScope class implementation ////

    EntityScope::EntityScope(Entity& t)
        : entity(t)
    {
    }

    EntityScope::~EntityScope()
    {
        entity.destroy();
    }

    //// ECS class implementation ////

    ECS::~ECS()
    {
        clear();
    }

    void ECS::clear()
    {
        if (is_empty())
            return;

        _registry.clear();
    }

    bool ECS::is_empty()
    {
        return get_all_entities().empty();
    }

    Entity ECS::create_entity(
        const std::string& name,
        const std::string& tag,
        const std::string& layer,
        const Uuid& parent)
    {
        EntityDescription desc = {};
        desc.name = name;
        desc.tag = tag;
        desc.layer = layer;
        desc.parent = parent;
        return Entity(_registry, desc);
    }

    Entity ECS::create_entity(const EntityDescription& desc)
    {
        return Entity(_registry, desc);
    }

    Entity ECS::get_entity(const Uuid& id)
    {
        return Entity(_registry, static_cast<EntityHandle>(id.value));
    }

    std::vector<Entity> ECS::get_all_entities()
    {
        std::vector<Entity> toys = {};
        auto view = _registry.view<EntityDescription>();
        for (auto entity : view)
            toys.emplace_back(_registry, entity);
        return toys;
    }
}
