#include "tbx/ecs/entity_registry.h"
#include "tbx/ecs/entity.h"

namespace tbx
{
    using EntityHandle = entt::entity;

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
        Uuid value = {};
    };

    template <typename TComponent, typename TValue>
    static void set_component_value(
        entt::registry& registry,
        const Uuid& id,
        TValue&& value)
    {
        auto entityHandle = static_cast<EntityHandle>(id.value);
        if (!registry.valid(entityHandle))
            return;

        if (!registry.all_of<TComponent>(entityHandle))
        {
            registry.emplace<TComponent>(
                entityHandle,
                TComponent {.value = std::forward<TValue>(value)});
            return;
        }

        registry.get<TComponent>(entityHandle).value = std::forward<TValue>(value);
    }

    template <typename TComponent, typename TValue>
    static TValue get_component_value(
        const entt::registry& registry,
        const Uuid& id)
    {
        auto entityHandle = static_cast<EntityHandle>(id.value);
        if (!registry.valid(entityHandle))
            return {};

        if (!registry.all_of<TComponent>(entityHandle))
            return {};

        return registry.get<TComponent>(entityHandle).value;
    }

    EntityRegistry::EntityRegistry()
        : _impl(std::make_unique<entt::registry>())
    {
    }

    EntityRegistry::~EntityRegistry() noexcept = default;

    bool EntityRegistry::is_empty() const
    {
        auto view = _impl->view<EntityNameComponent>();
        return view.empty();
    }

    void EntityRegistry::clear()
    {
        _impl->clear();
    }

    Uuid EntityRegistry::add(
        const std::string& name,
        const std::string& tag,
        const std::string& layer,
        const Uuid& parent)
    {
        EntityHandle handle = _impl->create();

        auto id = static_cast<uint32>(entt::to_integral(handle));
        auto resolvedName = name;
        if (resolvedName.empty())
            resolvedName = tbx::to_string(Uuid(id));

        _impl->emplace<EntityNameComponent>(handle, EntityNameComponent {.value = resolvedName});
        _impl->emplace<EntityTagComponent>(handle, EntityTagComponent {.value = tag});
        _impl->emplace<EntityLayerComponent>(handle, EntityLayerComponent {.value = layer});
        _impl->emplace<EntityParentComponent>(handle, EntityParentComponent {.value = parent});

        return Uuid(id);
    }

    void EntityRegistry::remove(Entity& entity)
    {
        if (entity._registry != this)
            return;

        auto handle = static_cast<EntityHandle>(entity._id.value);
        if (!_impl->valid(handle))
            return;

        _impl->destroy(handle);
        entity._id = {};
        entity._registry = nullptr;
    }

    Entity EntityRegistry::get(const Uuid& id)
    {
        if (!_impl->valid(static_cast<EntityHandle>(id.value)))
            return {};

        return Entity(*this, id, Entity::FromRegistryTag {});
    }

    std::vector<Entity> EntityRegistry::get_all()
    {
        std::vector<Entity> entities = {};
        auto view = _impl->view<EntityNameComponent>();

        for (const auto entityHandle : view)
        {
            auto id = Uuid(static_cast<uint32>(entt::to_integral(entityHandle)));
            entities.push_back(Entity(*this, id, Entity::FromRegistryTag {}));
        }

        return entities;
    }

    void EntityRegistry::for_each(const std::function<void(Entity&)>& callback)
    {
        if (!callback)
            return;

        auto view = _impl->view<EntityNameComponent>();
        for (const auto entityHandle : view)
        {
            auto id = Uuid(static_cast<uint32>(entt::to_integral(entityHandle)));
            Entity entity(*this, id, Entity::FromRegistryTag {});
            callback(entity);
        }
    }

    std::string EntityRegistry::get_name(const Uuid& id) const
    {
        return get_component_value<EntityNameComponent, std::string>(*_impl, id);
    }

    void EntityRegistry::set_name(const Uuid& id, const std::string& name)
    {
        set_component_value<EntityNameComponent>(*_impl, id, name);
    }

    std::string EntityRegistry::get_tag(const Uuid& id) const
    {
        return get_component_value<EntityTagComponent, std::string>(*_impl, id);
    }

    void EntityRegistry::set_tag(const Uuid& id, const std::string& tag)
    {
        set_component_value<EntityTagComponent>(*_impl, id, tag);
    }

    Uuid EntityRegistry::get_parent_id(const Uuid& id) const
    {
        return get_component_value<EntityParentComponent, Uuid>(*_impl, id);
    }

    void EntityRegistry::set_parent_id(const Uuid& id, const Uuid& parent)
    {
        set_component_value<EntityParentComponent>(*_impl, id, parent);
    }

    std::string EntityRegistry::get_layer(const Uuid& id) const
    {
        return get_component_value<EntityLayerComponent, std::string>(*_impl, id);
    }

    void EntityRegistry::set_layer(const Uuid& id, const std::string& layer)
    {
        set_component_value<EntityLayerComponent>(*_impl, id, layer);
    }
}
