#pragma once
#include "tbx/debugging/macros.h"
#include "tbx/ecs/entity_registry.h"
#include <utility>

namespace tbx
{
    template <typename TComponent>
    TComponent& Entity::add_component(const TComponent& component)
    {
        TBX_ASSERT(_registry != nullptr, "Cannot add a component to an unbound entity.");
        TBX_ASSERT(
            _registry != nullptr && _registry->has(_id),
            "Cannot add a component to a stale entity handle.");
        return _registry->add<TComponent>(_id, component);
    }

    template <typename TComponent, typename... TArgs>
    TComponent& Entity::add_component(TArgs&&... args)
    {
        TBX_ASSERT(_registry != nullptr, "Cannot add a component to an unbound entity.");
        TBX_ASSERT(
            _registry != nullptr && _registry->has(_id),
            "Cannot add a component to a stale entity handle.");
        return _registry->add<TComponent>(_id, std::forward<TArgs>(args)...);
    }

    template <typename TComponent>
    void Entity::remove_component()
    {
        if (_registry == nullptr)
            return;
        if (!_registry->has(_id))
        {
            TBX_ASSERT(false, "Cannot remove a component from a stale entity handle.");
            return;
        }

        _registry->remove<TComponent>(_id);
    }

    template <typename... TComponent>
    decltype(auto) Entity::get_components() const
    {
        TBX_ASSERT(_registry != nullptr, "Cannot read components from an unbound entity.");
        TBX_ASSERT(
            _registry != nullptr && _registry->has(_id),
            "Cannot read components from a stale entity handle.");
        TBX_ASSERT(
            (_registry->has<TComponent>(_id) && ...),
            "Cannot read missing component(s) from an entity.");
        return _registry->get_with<TComponent...>(_id);
    }

    template <typename TComponent>
    TComponent& Entity::get_component() const
    {
        TBX_ASSERT(_registry != nullptr, "Cannot read a component from an unbound entity.");
        TBX_ASSERT(
            _registry != nullptr && _registry->has(_id),
            "Cannot read a component from a stale entity handle.");
        TBX_ASSERT(
            _registry != nullptr && _registry->has<TComponent>(_id),
            "Cannot read a missing component from an entity.");
        return _registry->get_with<TComponent>(_id);
    }

    template <typename TComponent>
    bool Entity::has_component() const
    {
        if (_registry == nullptr)
            return false;
        if (!_registry->has(_id))
        {
            TBX_ASSERT(false, "Cannot query a component from a stale entity handle.");
            return false;
        }

        return _registry->has<TComponent>(_id);
    }
}


