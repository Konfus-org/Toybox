#include "tbx/ecs/entities.h"
#include "tbx/common/uuid.h"

namespace tbx
{
    struct EntityNameComponent
    {
        std::string value = "";
    };

    struct EntityTagComponent
    {
        std::string value = "";
    };

    struct EntityLayerComponent
    {
        std::string value = "";
    };

    struct EntityParentComponent
    {
        Uuid value = Uuid::NONE;
    };

    //// Entity class implementation ////

    Entity::Entity(EntityRegistry& registry, const EntityHandle& handle)
        : _registry(&registry)
        , _handle(handle)
    {
    }

    Entity::Entity(const std::string& name, EntityRegistry& registry, const Uuid& parent)
        : _registry(&registry)
        , _handle(registry.register(name, "", "", parent))
    {
    }

    Entity::Entity(EntityRegistry& registry, const Uuid& parent)
        : Entity("", registry, parent)
    {
    }

    void Entity::destroy()
    {
        if (_registry == nullptr)
            return;

        _registry->unregister(*this);
    }

    Uuid Entity::get_id() const
    {
        return static_cast<uint32>(_handle);
    }

    std::string Entity::get_name() const
    {
        if (_registry == nullptr)
            return "";

        return _registry->get_entity_name(_handle);
    }

    void Entity::set_name(const std::string& name)
    {
        if (_registry == nullptr)
            return;

        _registry->set_entity_name(_handle, name);
    }

    std::string Entity::get_tag() const
    {
        if (_registry == nullptr)
            return "";

        return _registry->get_entity_tag(_handle);
    }

    void Entity::set_tag(const std::string& tag)
    {
        if (_registry == nullptr)
            return;

        _registry->set_entity_tag(_handle, tag);
    }

    std::string Entity::get_layer() const
    {
        if (_registry == nullptr)
            return "";

        return _registry->get_entity_layer(_handle);
    }

    void Entity::set_layer(const std::string& layer)
    {
        if (_registry == nullptr)
            return;

        _registry->set_entity_layer(_handle, layer);
    }

    Uuid Entity::get_parent() const
    {
        if (_registry == nullptr)
            return invalid::uuid;

        return _registry->get_entity_parent(_handle);
    }

    void Entity::set_parent(const Uuid& parent)
    {
        if (_registry == nullptr)
            return;

        _registry->set_entity_parent(_handle, parent);
    }

    std::string to_string(const Entity& entity)
    {
        std::string value = "Entity{";
        value += "id=" + std::to_string(entity.get_id().value);
        value += ", name='" + entity.get_name() + "'";
        value += ", tag='" + entity.get_tag() + "'";
        value += ", layer='" + entity.get_layer() + "'";
        value += ", parent=" + std::to_string(entity.get_parent().value);
        value += "}";
        return value;
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
        unregister(entity);
    }

    void EntityRegistry::destroy_all()
    {
        _pool->clear();
        _pool = std::make_unique<EntityPool>();
    }

    bool EntityRegistry::is_empty() const
    {
        auto view = _pool->view<EntityNameComponent>();
        return view.empty();
    }

    EntityHandle EntityRegistry::register(
        const std::string& name,
        const std::string& tag,
        const std::string& layer,
        const Uuid& parent)
    {
        EntityHandle handle = _pool->create();

        Uuid id = static_cast<uint32>(handle);
        std::string resolved_name = name;
        if (resolved_name.empty())
            resolved_name = to_string(id);

        _pool->emplace<EntityNameComponent>(handle, resolved_name);
        _pool->emplace<EntityTagComponent>(handle, tag);
        _pool->emplace<EntityLayerComponent>(handle, layer);
        _pool->emplace<EntityParentComponent>(handle, parent);

        return handle;
    }

    void EntityRegistry::unregister(const EntityHandle& handle)
    {
        if (!_pool->valid(handle))
            return;

        _pool->destroy(handle);
    }

    void EntityRegistry::unregister(Entity& entity)
    {
        if (entity._registry != this)
            return;

        unregister(entity._handle);
        entity._handle = entt::null;
    }

    Entity EntityRegistry::get(const Uuid& id)
    {
        return Entity(*this, static_cast<EntityHandle>(id.value));
    }

    std::vector<Entity> EntityRegistry::get_all()
    {
        std::vector<Entity> entities = {};
        auto view = _pool->view<EntityNameComponent>();
        for (auto entity : view)
            entities.emplace_back(*this, entity);
        return entities;
    }

    void EntityRegistry::for_each(const std::function<void(Entity&)>& callback)
    {
        auto view = _pool->view<EntityNameComponent>();
        for (auto entity_handle : view)
        {
            Entity entity(*this, entity_handle);
            callback(entity);
        }
    }

    std::string EntityRegistry::get_entity_name(const EntityHandle& handle) const
    {
        if (!_pool->valid(handle))
            return "";

        if (!_pool->all_of<EntityNameComponent>(handle))
            return "";

        return _pool->get<EntityNameComponent>(handle).value;
    }

    void EntityRegistry::set_entity_name(const EntityHandle& handle, const std::string& name)
    {
        if (!_pool->valid(handle))
            return;

        if (!_pool->all_of<EntityNameComponent>(handle))
            _pool->emplace<EntityNameComponent>(handle, name);
        else
            _pool->get<EntityNameComponent>(handle).value = name;
    }

    std::string EntityRegistry::get_entity_tag(const EntityHandle& handle) const
    {
        if (!_pool->valid(handle))
            return "";

        if (!_pool->all_of<EntityTagComponent>(handle))
            return "";

        return _pool->get<EntityTagComponent>(handle).value;
    }

    void EntityRegistry::set_entity_tag(const EntityHandle& handle, const std::string& tag)
    {
        if (!_pool->valid(handle))
            return;

        if (!_pool->all_of<EntityTagComponent>(handle))
            _pool->emplace<EntityTagComponent>(handle, tag);
        else
            _pool->get<EntityTagComponent>(handle).value = tag;
    }

    std::string EntityRegistry::get_entity_layer(const EntityHandle& handle) const
    {
        if (!_pool->valid(handle))
            return "";

        if (!_pool->all_of<EntityLayerComponent>(handle))
            return "";

        return _pool->get<EntityLayerComponent>(handle).value;
    }

    void EntityRegistry::set_entity_layer(const EntityHandle& handle, const std::string& layer)
    {
        if (!_pool->valid(handle))
            return;

        if (!_pool->all_of<EntityLayerComponent>(handle))
            _pool->emplace<EntityLayerComponent>(handle, layer);
        else
            _pool->get<EntityLayerComponent>(handle).value = layer;
    }

    Uuid EntityRegistry::get_entity_parent(const EntityHandle& handle) const
    {
        if (!_pool->valid(handle))
            return invalid::uuid;

        if (!_pool->all_of<EntityParentComponent>(handle))
            return invalid::uuid;

        return _pool->get<EntityParentComponent>(handle).value;
    }

    void EntityRegistry::set_entity_parent(const EntityHandle& handle, const Uuid& parent)
    {
        if (!_pool->valid(handle))
            return;

        if (!_pool->all_of<EntityParentComponent>(handle))
            _pool->emplace<EntityParentComponent>(handle, parent);
        else
            _pool->get<EntityParentComponent>(handle).value = parent;
    }
}
