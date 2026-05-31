#pragma once

namespace tbx
{
    template <typename TComponent>
        requires std::derived_from<TComponent, Component>
    bool World::has(const Uuid& id) const
    {
        return _registry.has<TComponent>(id);
    }

    template <typename... TComponent>
        requires(std::derived_from<TComponent, Component> && ...)
    std::vector<Entity> World::get_with() const
    {
        return _registry.get_with<TComponent...>();
    }

    template <typename... TComponent>
        requires(std::derived_from<TComponent, Component> && ...)
    Entity World::first_with() const
    {
        return _registry.first_with<TComponent...>();
    }

    template <typename... TComponent>
        requires(std::derived_from<TComponent, Component> && ...)
    void World::for_each_with(const std::function<void(Entity&)>& callback)
    {
        _registry.for_each_with<TComponent...>(callback);
    }
}
