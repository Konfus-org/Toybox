#include "systems/ecs/internal/entity_registry_internal.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/registry.h"
#include <format>
#include <mutex>

namespace tbx
{
    EntityRegistry::EntityRegistry()
        : _impl(std::make_unique<entt::registry>())
    {
    }

    EntityRegistry::~EntityRegistry() noexcept = default;

    bool EntityRegistry::is_empty() const
    {
        auto guard = std::shared_lock(_mutex);
        auto view = _impl->view<internal::EntityNameComponent>();
        return view.empty();
    }

    void EntityRegistry::clear()
    {
        auto guard = std::unique_lock(_mutex);
        _impl = std::make_unique<entt::registry>();
    }

    bool EntityRegistry::has(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        return _impl->valid(internal::to_entity_handle(id));
    }

    Uuid EntityRegistry::add(
        const std::string& name,
        const std::string& tag,
        const std::string& layer,
        const Uuid& parent)
    {
        auto guard = std::unique_lock(_mutex);
        const internal::EntityHandle handle = _impl->create();
        const auto id = internal::to_entity_id(handle);

        auto resolvedName = name;
        if (resolvedName.empty())
            resolvedName = std::format("{}", id);

        _impl->emplace<internal::EntityNameComponent>(
            handle,
            internal::EntityNameComponent {.value = resolvedName});
        _impl->emplace<internal::EntityTagComponent>(
            handle,
            internal::EntityTagComponent {.value = tag});
        _impl->emplace<internal::EntityLayerComponent>(
            handle,
            internal::EntityLayerComponent {.value = layer});
        _impl->emplace<internal::EntityParentComponent>(
            handle,
            internal::EntityParentComponent {.value = parent});

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
        const internal::EntityHandle handle = internal::to_entity_handle(id);
        if (!_impl->valid(handle))
            static_cast<void>(_impl->create(handle));

        auto resolvedName = name;
        if (resolvedName.empty())
            resolvedName = std::format("{}", id);

        _impl->emplace_or_replace<internal::EntityNameComponent>(
            handle,
            internal::EntityNameComponent {.value = resolvedName});
        _impl->emplace_or_replace<internal::EntityTagComponent>(
            handle,
            internal::EntityTagComponent {.value = tag});
        _impl->emplace_or_replace<internal::EntityLayerComponent>(
            handle,
            internal::EntityLayerComponent {.value = layer});
        _impl->emplace_or_replace<internal::EntityParentComponent>(
            handle,
            internal::EntityParentComponent {.value = parent});

        return id;
    }

    void EntityRegistry::remove(Entity& entity)
    {
        auto guard = std::unique_lock(_mutex);
        if (!entity._registry.has_value() || &entity._registry->get() != this)
            return;

        auto handle = internal::to_entity_handle(entity._id);
        if (!_impl->valid(handle))
        {
            TBX_ASSERT(false, "Attempted to remove a stale entity handle from the registry.");
            return;
        }

        _impl->destroy(handle);
        entity._id = {};
        entity._registry = std::nullopt;
    }

    Entity EntityRegistry::get(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        if (!_impl->valid(internal::to_entity_handle(id)))
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
        auto view = _impl->view<internal::EntityNameComponent>();

        for (const auto entityHandle : view)
        {
            auto id = internal::to_entity_id(entityHandle);
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
            auto view = _impl->view<internal::EntityNameComponent>();
            for (const auto entityHandle : view)
                ids.push_back(internal::to_entity_id(entityHandle));
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
        return internal::get_component_value<internal::EntityNameComponent, std::string>(
            *_impl,
            id);
    }

    void EntityRegistry::set_name(const Uuid& id, const std::string& name)
    {
        auto guard = std::unique_lock(_mutex);
        internal::set_component_value<internal::EntityNameComponent>(*_impl, id, name);
    }

    std::string EntityRegistry::get_tag(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        return internal::get_component_value<internal::EntityTagComponent, std::string>(*_impl, id);
    }

    void EntityRegistry::set_tag(const Uuid& id, const std::string& tag)
    {
        auto guard = std::unique_lock(_mutex);
        internal::set_component_value<internal::EntityTagComponent>(*_impl, id, tag);
    }

    Uuid EntityRegistry::get_parent_id(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        return internal::get_component_value<internal::EntityParentComponent, Uuid>(*_impl, id);
    }

    void EntityRegistry::set_parent_id(const Uuid& id, const Uuid& parent)
    {
        auto guard = std::unique_lock(_mutex);
        internal::set_component_value<internal::EntityParentComponent>(*_impl, id, parent);
    }

    std::string EntityRegistry::get_layer(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        return internal::get_component_value<internal::EntityLayerComponent, std::string>(
            *_impl,
            id);
    }

    void EntityRegistry::set_layer(const Uuid& id, const std::string& layer)
    {
        auto guard = std::unique_lock(_mutex);
        internal::set_component_value<internal::EntityLayerComponent>(*_impl, id, layer);
    }
}
