#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/registry.h"
#include "tbx/systems/plugin_api/plugin_ownership_tracking.h"
#include <vector>

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

    static EntityHandle to_entity_handle(const Uuid& id)
    {
        if (!id.is_valid())
            return entt::null;

        return static_cast<EntityHandle>(id.value - 1U);
    }

    static Uuid to_entity_id(EntityHandle handle)
    {
        const auto handle_value = static_cast<uint32>(entt::to_integral(handle));
        return Uuid(handle_value + 1U);
    }

    template <typename TComponent, typename TValue>
    static void set_component_value(entt::registry& registry, const Uuid& id, TValue&& value)
    {
        auto entityHandle = to_entity_handle(id);
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
    static TValue get_component_value(const entt::registry& registry, const Uuid& id)
    {
        auto entityHandle = to_entity_handle(id);
        if (!registry.valid(entityHandle))
            return {};

        if (!registry.all_of<TComponent>(entityHandle))
            return {};

        return registry.get<TComponent>(entityHandle).value;
    }

    EntityRegistry::EntityRegistry()
        : _registry(std::make_unique<entt::registry>())
    {
    }

    EntityRegistry::~EntityRegistry() noexcept = default;

    bool EntityRegistry::is_empty() const
    {
        auto guard = std::shared_lock(_mutex);
        auto view = _registry->view<EntityNameComponent>();
        return view.empty();
    }

    void EntityRegistry::clear()
    {
        auto guard = std::unique_lock(_mutex);
        _registry = std::make_unique<entt::registry>();
    }

    bool EntityRegistry::has(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        return _registry->valid(to_entity_handle(id));
    }

    Uuid EntityRegistry::add(
        const std::string& name,
        const std::string& tag,
        const std::string& layer,
        const Uuid& parent)
    {
        auto guard = std::unique_lock(_mutex);
        const EntityHandle handle = _registry->create();
        const auto id = to_entity_id(handle);

        auto resolvedName = name;
        if (resolvedName.empty())
            resolvedName = std::format("{}", id);

        _registry->emplace<EntityNameComponent>(
            handle,
            EntityNameComponent {.value = resolvedName});
        _registry->emplace<EntityTagComponent>(handle, EntityTagComponent {.value = tag});
        _registry->emplace<EntityLayerComponent>(handle, EntityLayerComponent {.value = layer});
        _registry->emplace<EntityParentComponent>(handle, EntityParentComponent {.value = parent});

        track_plugin_owned_entity(id);

        return id;
    }

    Uuid EntityRegistry::add(
        const Uuid& id,
        const std::string& name,
        const std::string& tag,
        const std::string& layer,
        const Uuid& parent)
    {
        if (!id.is_valid())
            return add(name, tag, layer, parent);

        auto guard = std::unique_lock(_mutex);
        const EntityHandle handle = to_entity_handle(id);
        if (!_registry->valid(handle))
            static_cast<void>(_registry->create(handle));

        auto resolvedName = name;
        if (resolvedName.empty())
            resolvedName = std::format("{}", id);

        _registry->emplace_or_replace<EntityNameComponent>(
            handle,
            EntityNameComponent {.value = resolvedName});
        _registry->emplace_or_replace<EntityTagComponent>(
            handle,
            EntityTagComponent {.value = tag});
        _registry->emplace_or_replace<EntityLayerComponent>(
            handle,
            EntityLayerComponent {.value = layer});
        _registry->emplace_or_replace<EntityParentComponent>(
            handle,
            EntityParentComponent {.value = parent});

        track_plugin_owned_entity(id);

        return id;
    }

    void EntityRegistry::remove(Entity& entity)
    {
        auto guard = std::unique_lock(_mutex);
        if (!entity._registry.has_value() || &entity._registry->get() != this)
            return;

        auto handle = to_entity_handle(entity._id);
        if (!_registry->valid(handle))
        {
            TBX_ASSERT(false, "Attempted to remove a stale entity handle from the registry.");
            return;
        }

        _registry->destroy(handle);
        entity._id = {};
        entity._registry = std::nullopt;
    }

    Entity EntityRegistry::get(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        if (!_registry->valid(to_entity_handle(id)))
            return {};

        auto entity = Entity {};
        entity._id = id;
        entity._registry = std::ref(const_cast<EntityRegistry&>(*this));
        return entity;
    }

    std::vector<Entity> EntityRegistry::get_all() const
    {
        auto guard = std::shared_lock(_mutex);
        std::vector<Entity> entities = {};
        auto view = _registry->view<EntityNameComponent>();

        for (const auto entityHandle : view)
        {
            auto id = to_entity_id(entityHandle);
            auto entity = Entity {};
            entity._id = id;
            entity._registry = std::ref(const_cast<EntityRegistry&>(*this));
            entities.push_back(entity);
        }

        return entities;
    }

    void EntityRegistry::for_each(const std::function<void(Entity&)>& callback)
    {
        if (!callback)
            return;

        auto ids = std::vector<Uuid> {};
        {
            auto guard = std::shared_lock(_mutex);
            auto view = _registry->view<EntityNameComponent>();
            for (const auto entityHandle : view)
                ids.push_back(to_entity_id(entityHandle));
        }

        for (const auto& id : ids)
        {
            auto entity = get(id);
            callback(entity);
        }
    }

    std::string EntityRegistry::get_name(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        return get_component_value<EntityNameComponent, std::string>(*_registry, id);
    }

    void EntityRegistry::set_name(const Uuid& id, const std::string& name)
    {
        auto guard = std::unique_lock(_mutex);
        set_component_value<EntityNameComponent>(*_registry, id, name);
    }

    std::string EntityRegistry::get_tag(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        return get_component_value<EntityTagComponent, std::string>(*_registry, id);
    }

    void EntityRegistry::set_tag(const Uuid& id, const std::string& tag)
    {
        auto guard = std::unique_lock(_mutex);
        set_component_value<EntityTagComponent>(*_registry, id, tag);
    }

    Uuid EntityRegistry::get_parent_id(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        return get_component_value<EntityParentComponent, Uuid>(*_registry, id);
    }

    void EntityRegistry::set_parent_id(const Uuid& id, const Uuid& parent)
    {
        auto guard = std::unique_lock(_mutex);
        set_component_value<EntityParentComponent>(*_registry, id, parent);
    }

    std::string EntityRegistry::get_layer(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        return get_component_value<EntityLayerComponent, std::string>(*_registry, id);
    }

    void EntityRegistry::set_layer(const Uuid& id, const std::string& layer)
    {
        auto guard = std::unique_lock(_mutex);
        set_component_value<EntityLayerComponent>(*_registry, id, layer);
    }
}
