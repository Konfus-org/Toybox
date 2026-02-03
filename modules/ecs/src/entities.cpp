#include "tbx/ecs/entities.h"

namespace tbx
{
    //// Entity class implementation ////

    Entity::Entity(EntityPool* pool, const EntityHandle& handle)
        : _pool(pool)
        , _handle(handle)
    {
    }

    Entity::Entity(EntityPool* pools, const EntityDescription& desc)
        : _pool(pools)
        , _handle(pools->create())
    {
        _pool->emplace<EntityDescription>(_handle, desc);
    }

    void Entity::destroy()
    {
        _pool->destroy(_handle);
    }

    Uuid Entity::get_id() const
    {
        return static_cast<uint32>(_handle);
    }

    EntityDescription& Entity::get_description() const
    {
        return _pool->get<EntityDescription>(_handle);
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

    //// EntityRegistry class implementation ////

    EntityRegistry::EntityRegistry()
        : _pool(std::make_unique<EntityPool>())
    {
    }

    EntityRegistry::~EntityRegistry() noexcept
    {
        destroy_all();
    }

    void EntityRegistry::destroy(Entity& entity)
    {
        entity.destroy();
    }

    void EntityRegistry::destroy_all()
    {
        _pool->clear();
        _pool = std::make_unique<EntityPool>();
    }

    bool EntityRegistry::is_empty()
    {
        return get_all().empty();
    }

    Entity EntityRegistry::create(
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
        return Entity(_pool.get(), desc);
    }

    Entity EntityRegistry::create(const EntityDescription& desc)
    {
        return Entity(_pool.get(), desc);
    }

    Entity EntityRegistry::get(const Uuid& id)
    {
        return Entity(_pool.get(), static_cast<EntityHandle>(id.value));
    }

    std::vector<Entity> EntityRegistry::get_all()
    {
        std::vector<Entity> toys = {};
        auto view = _pool->view<EntityDescription>();
        for (auto entity : view)
            toys.emplace_back(_pool.get(), entity);
        return toys;
    }

    void EntityRegistry::for_each(const std::function<void(Entity&)>& callback)
    {
        auto view = _pool->view<EntityDescription>();
        for (auto entity_handle : view)
        {
            Entity entity(_pool.get(), entity_handle);
            callback(entity);
        }
    }
}
