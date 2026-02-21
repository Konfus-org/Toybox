#pragma once
#include "tbx/ecs/entity_registry.h"
#include <utility>

namespace tbx
{
    template <typename TComponent>
    TComponent& Entity::add_component(const TComponent& component)
    {
        return _registry->add<TComponent>(_id, component);
    }

    template <typename TComponent, typename... TArgs>
    TComponent& Entity::add_component(TArgs&&... args)
    {
        return _registry->add<TComponent>(_id, std::forward<TArgs>(args)...);
    }

    template <typename TComponent>
    void Entity::remove_component()
    {
        if (_registry == nullptr)
            return;

        _registry->remove<TComponent>(_id);
    }

    template <typename... TComponent>
    decltype(auto) Entity::get_components() const
    {
        return _registry->get_with<TComponent...>(_id);
    }

    template <typename TComponent>
    TComponent& Entity::get_component() const
    {
        return _registry->get_with<TComponent>(_id);
    }

    template <typename TComponent>
    bool Entity::has_component() const
    {
        if (_registry == nullptr)
            return false;

        return _registry->has<TComponent>(_id);
    }
}
