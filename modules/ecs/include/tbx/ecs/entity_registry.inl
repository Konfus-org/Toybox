#pragma once

namespace tbx
{
    template <typename TComponent, typename... TArgs>
    TComponent& EntityRegistry::add(const Uuid& id, TArgs&&... args)
    {
        auto handle = static_cast<entt::entity>(id.value);
        return _impl->emplace_or_replace<TComponent>(handle, std::forward<TArgs>(args)...);
    }

    template <typename TComponent>
    void EntityRegistry::remove(const Uuid& id)
    {
        auto handle = static_cast<entt::entity>(id.value);
        if (!_impl->valid(handle))
            return;

        _impl->remove<TComponent>(handle);
    }

    template <typename... TComponent>
    std::vector<Entity> EntityRegistry::get_with()
    {
        std::vector<Entity> entities = {};
        auto view = _impl->view<TComponent...>();
        for (const auto entityHandle : view)
        {
            auto id = Uuid(static_cast<uint32>(entt::to_integral(entityHandle)));
            entities.push_back(get(id));
        }

        return entities;
    }

    template <typename... TComponent>
    void EntityRegistry::for_each_with(const std::function<void(Entity&)>& callback)
    {
        if (!callback)
            return;

        auto view = _impl->view<TComponent...>();
        for (const auto entityHandle : view)
        {
            auto id = Uuid(static_cast<uint32>(entt::to_integral(entityHandle)));
            auto entity = get(id);
            callback(entity);
        }
    }

    template <typename... TComponent>
    decltype(auto) EntityRegistry::get_with(const Uuid& id) const
    {
        auto handle = static_cast<entt::entity>(id.value);
        return _impl->get<TComponent...>(handle);
    }

    template <typename TComponent>
    bool EntityRegistry::has(const Uuid& id) const
    {
        auto handle = static_cast<entt::entity>(id.value);
        if (!_impl->valid(handle))
            return false;

        return _impl->all_of<TComponent>(handle);
    }
}
