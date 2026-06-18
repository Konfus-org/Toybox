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

    // Explicit sibling order, so the editor can present and persist a user-chosen arrangement rather than
    // entt's insertion order. Lower sorts first among entities sharing a parent.
    struct EntityOrderComponent
    {
        int value = 0;
    };

    // Wholesale enable flag for the entity. A disabled entity is skipped by the typed component queries
    // (get_with / first_with / for_each_with), so every runtime system that gathers through them —
    // rendering, physics, scripting — passes it over, while the editor (get_all / get / serialize) still
    // sees it so it can be listed and re-enabled. Defaults enabled.
    struct EntityEnabledComponent
    {
        bool value = true;
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

    EntityRegistry::EntityRegistry(const EntityRegistry& other)
        : _registry(std::make_unique<entt::registry>())
    {
        for (const auto& entity : other.get_all())
            absorb(entity);
    }

    EntityRegistry& EntityRegistry::operator=(const EntityRegistry& other)
    {
        if (this == &other)
            return *this;

        clear();
        for (const auto& entity : other.get_all())
            absorb(entity);
        return *this;
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
        _registry->emplace<EntityOrderComponent>(handle, EntityOrderComponent {});
        _registry->emplace<EntityEnabledComponent>(handle, EntityEnabledComponent {});

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
        {
            const auto created_handle = _registry->create(handle);
            TBX_ASSERT(created_handle == handle, "Failed to create entity handle '{}'.", id);
        }

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
        // Order defaults to 0 here; a persisted value is applied afterwards via set_order_value (the
        // add overloads carry only the four identity fields).
        if (!_registry->all_of<EntityOrderComponent>(handle))
            _registry->emplace<EntityOrderComponent>(handle, EntityOrderComponent {});
        // Enabled defaults true here; a persisted value is applied afterwards via set_enabled, mirroring
        // order. Preserved if the entity already exists so a plain re-add doesn't silently re-enable it.
        if (!_registry->all_of<EntityEnabledComponent>(handle))
            _registry->emplace<EntityEnabledComponent>(handle, EntityEnabledComponent {});

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

    int EntityRegistry::get_order_value(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        return get_component_value<EntityOrderComponent, int>(*_registry, id);
    }

    void EntityRegistry::set_order_value(const Uuid& id, int order)
    {
        auto guard = std::unique_lock(_mutex);
        set_component_value<EntityOrderComponent>(*_registry, id, order);
    }

    bool EntityRegistry::get_enabled(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        const auto handle = to_entity_handle(id);
        // Absent flag means enabled: every entity gets one on add, but treating absence as enabled keeps
        // any entity created outside the add path (and the query filter) from vanishing.
        if (!_registry->valid(handle) || !_registry->all_of<EntityEnabledComponent>(handle))
            return true;

        return _registry->get<EntityEnabledComponent>(handle).value;
    }

    void EntityRegistry::set_enabled(const Uuid& id, bool enabled)
    {
        auto guard = std::unique_lock(_mutex);
        set_component_value<EntityEnabledComponent>(*_registry, id, enabled);
    }

    bool EntityRegistry::is_handle_enabled(entt::entity handle) const
    {
        // Mirrors get_enabled but takes a resolved handle and does not lock: callers hold _mutex.
        // Absent flag means enabled, matching every other entity-enabled read path.
        if (!_registry->valid(handle) || !_registry->all_of<EntityEnabledComponent>(handle))
            return true;

        return _registry->get<EntityEnabledComponent>(handle).value;
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

    void EntityRegistry::absorb(const Entity& source)
    {
        const auto id = source.get_id();
        if (!id.is_valid() || !source._registry.has_value())
            return;

        auto& source_registry = source._registry->get();
        if (&source_registry == this)
            return;

        // Recreate identity metadata in this registry, then copy each registered component by value.
        add(id, source.get_name(), source.get_tag(), source.get_layer(), source.get_parent());
        set_order_value(id, source.get_order());
        set_enabled(id, source.is_enabled());

        const auto entries = get_entity_component_type_registrations();
        const auto handle = to_entity_handle(id);
        auto source_guard = std::shared_lock(source_registry._mutex);
        auto guard = std::unique_lock(_mutex);
        for (const auto& entry : entries)
        {
            const auto* storage = source_registry._registry->storage(entry.type_id);
            if (storage == nullptr || !storage->contains(handle) || !entry.copy_value)
                continue;

            entry.copy_value(storage->value(handle), *_registry, handle);
        }
    }
}
