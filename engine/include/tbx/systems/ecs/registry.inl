#pragma once
#include <mutex>

namespace tbx
{
    template <typename TComponent, typename... TArgs>
        requires std::derived_from<TComponent, Component>
    TComponent& EntityRegistry::add(const Uuid& id, TArgs&&... args)
    {
        register_entity_component_type<TComponent>();
        auto guard = std::unique_lock(_mutex);
        auto handle = static_cast<entt::entity>(id.value - 1U);
        return _impl->emplace_or_replace<TComponent>(handle, std::forward<TArgs>(args)...);
    }

    template <typename TComponent>
        requires std::derived_from<TComponent, Component>
    void EntityRegistry::remove(const Uuid& id)
    {
        auto guard = std::unique_lock(_mutex);
        auto handle = static_cast<entt::entity>(id.value - 1U);
        if (!_impl->valid(handle))
            return;

        _impl->remove<TComponent>(handle);
    }

    template <typename... TComponent>
        requires(std::derived_from<TComponent, Component> && ...)
    std::vector<Entity> EntityRegistry::get_with() const
    {
        auto ids = std::vector<Uuid> {};
        {
            auto guard = std::shared_lock(_mutex);
            auto view = _impl->view<TComponent...>();
            for (const auto entityHandle : view)
            {
                const auto id = Uuid(static_cast<uint32>(entt::to_integral(entityHandle)) + 1U);
                ids.push_back(id);
            }
        }

        std::vector<Entity> entities = {};
        entities.reserve(ids.size());
        for (const auto& id : ids)
        {
            entities.push_back(get(id));
        }

        return entities;
    }

    template <typename... TComponent>
        requires(std::derived_from<TComponent, Component> && ...)
    Entity EntityRegistry::first_with() const
    {
        auto first_id = Uuid {};
        {
            auto guard = std::shared_lock(_mutex);
            auto view = _impl->view<TComponent...>();
            for (const auto entityHandle : view)
            {
                first_id = Uuid(static_cast<uint32>(entt::to_integral(entityHandle)) + 1U);
                break;
            }
        }

        if (!first_id.is_valid())
            return {};

        return get(first_id);
    }

    template <typename... TComponent>
        requires(std::derived_from<TComponent, Component> && ...)
    void EntityRegistry::for_each_with(const std::function<void(Entity&)>& callback)
    {
        if (!callback)
            return;

        auto ids = std::vector<Uuid> {};
        {
            auto guard = std::shared_lock(_mutex);
            auto view = _impl->view<TComponent...>();
            for (const auto entityHandle : view)
            {
                const auto id = Uuid(static_cast<uint32>(entt::to_integral(entityHandle)) + 1U);
                ids.push_back(id);
            }
        }

        for (const auto& id : ids)
        {
            auto entity = get(id);
            callback(entity);
        }
    }

    template <typename... TComponent>
        requires(std::derived_from<TComponent, Component> && ...)
    decltype(auto) EntityRegistry::get_with(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        auto handle = static_cast<entt::entity>(id.value - 1U);
        return _impl->get<TComponent...>(handle);
    }

    template <typename TComponent>
        requires std::derived_from<TComponent, Component>
    bool EntityRegistry::has(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        auto handle = static_cast<entt::entity>(id.value - 1U);
        if (!_impl->valid(handle))
            return false;

        return _impl->all_of<TComponent>(handle);
    }
}
