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

    //// EntityManager class implementation ////

    EntityManager::EntityManager()
        : _registry(std::make_unique<EntityRegistry>())
    {
    }

    EntityManager::~EntityManager() noexcept
    {
        destroy_all();
    }

    void EntityManager::destroy(Entity& entity)
    {
        entity.destroy();
    }

    void EntityManager::destroy_all()
    {
        _registry->clear();
        _registry = std::make_unique<EntityRegistry>();
    }

    bool EntityManager::is_empty()
    {
        return get_all().empty();
    }

    Entity EntityManager::create(
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

    Entity EntityManager::create(const EntityDescription& desc)
    {
        return Entity(_registry.get(), desc);
    }

    Entity EntityManager::get(const Uuid& id)
    {
        return Entity(_registry.get(), static_cast<EntityHandle>(id.value));
    }

    std::vector<Entity> EntityManager::get_all()
    {
        std::vector<Entity> toys = {};
        auto view = _registry->view<EntityDescription>();
        for (auto entity : view)
            toys.emplace_back(_registry.get(), entity);
        return toys;
    }

    void EntityManager::for_each(const std::function<void(Entity&)>& callback)
    {
        auto view = _registry->view<EntityDescription>();
        for (auto entity_handle : view)
        {
            Entity entity(_registry.get(), entity_handle);
            callback(entity);
        }
    }
}
