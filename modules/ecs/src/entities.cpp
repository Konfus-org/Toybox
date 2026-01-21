#include "tbx/ecs/entities.h"

namespace tbx
{
    //// Entity class implementation ////

    Entity::Entity(EntityRegistry* reg, const EntityHandle& handle)
        : _registry(reg)
        , _handle(handle)
    {
    }

    Entity::Entity(EntityRegistry* reg, const EntityDescription& desc)
        : _registry(reg)
        , _handle(reg->create())
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

    EntityScope::~EntityScope() noexcept
    {
        entity.destroy();
    }

    //// ECS class implementation ////

    ECS::ECS()
        : _registry(std::make_unique<EntityRegistry>())
    {
    }

    ECS::~ECS() noexcept
    {
        _registry->clear();
    }

    void ECS::clear()
    {
        _registry->clear();
        _registry = std::make_unique<EntityRegistry>();
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
        return Entity(_registry.get(), desc);
    }

    Entity ECS::create_entity(const EntityDescription& desc)
    {
        return Entity(_registry.get(), desc);
    }

    Entity ECS::get_entity(const Uuid& id)
    {
        return Entity(_registry.get(), static_cast<EntityHandle>(id.value));
    }

    std::vector<Entity> ECS::get_all_entities()
    {
        std::vector<Entity> toys = {};
        auto view = _registry->view<EntityDescription>();
        for (auto entity : view)
            toys.emplace_back(_registry.get(), entity);
        return toys;
    }
}
