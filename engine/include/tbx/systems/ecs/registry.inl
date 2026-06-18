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
        return _registry->emplace_or_replace<TComponent>(handle, std::forward<TArgs>(args)...);
    }

    template <typename TComponent>
        requires std::derived_from<TComponent, Component>
    void EntityRegistry::remove(const Uuid& id)
    {
        auto guard = std::unique_lock(_mutex);
        auto handle = static_cast<entt::entity>(id.value - 1U);
        if (!_registry->valid(handle))
            return;

        _registry->remove<TComponent>(handle);
    }

    template <typename... TComponent>
        requires(std::derived_from<TComponent, Component> && ...)
    std::vector<Entity> EntityRegistry::get_with() const
    {
        // Gather under a single shared lock: the view, the enabled filter, and the entity handles
        // are all read while holding it, so this avoids the per-entity re-locking (get_enabled + get)
        // and the intermediate id buffer that this hot query (rendering, physics, scripting) used to
        // pay on every call. Only trivial Entity handles are built in the loop — no callbacks run and
        // the lock is never re-acquired, so a writer simply waits until the gather completes.
        std::vector<Entity> entities = {};
        auto guard = std::shared_lock(_mutex);
        auto view = _registry->view<TComponent...>();
        for (const auto entityHandle : view)
        {
            // Disabled entities are turned off wholesale: skip them so no runtime system that gathers
            // through this query (rendering, physics, scripting) ever touches them. The editor lists
            // them via the unfiltered get_all / serialize path instead.
            if (!is_handle_enabled(entityHandle))
                continue;

            auto entity = Entity {};
            entity._id = Uuid(static_cast<uint32>(entt::to_integral(entityHandle)) + 1U);
            entity._registry = std::ref(const_cast<EntityRegistry&>(*this));
            entities.push_back(entity);
        }

        return entities;
    }

    template <typename... TComponent>
        requires(std::derived_from<TComponent, Component> && ...)
    Entity EntityRegistry::first_with() const
    {
        // The first *enabled* match: a disabled entity is turned off wholesale, so it can't be the one
        // a runtime system (e.g. the sky / post-processing / camera lookups) acts on. Filtered under
        // the same shared lock as the view, returning as soon as a match is found.
        auto guard = std::shared_lock(_mutex);
        auto view = _registry->view<TComponent...>();
        for (const auto entityHandle : view)
        {
            if (!is_handle_enabled(entityHandle))
                continue;

            auto entity = Entity {};
            entity._id = Uuid(static_cast<uint32>(entt::to_integral(entityHandle)) + 1U);
            entity._registry = std::ref(const_cast<EntityRegistry&>(*this));
            return entity;
        }

        return {};
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
            auto view = _registry->view<TComponent...>();
            for (const auto entityHandle : view)
            {
                const auto id = Uuid(static_cast<uint32>(entt::to_integral(entityHandle)) + 1U);
                ids.push_back(id);
            }
        }

        for (const auto& id : ids)
        {
            // Skip disabled entities so for_each_with-based systems (scripting) leave them wholly inert.
            if (!get_enabled(id))
                continue;

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
        return _registry->get<TComponent...>(handle);
    }

    template <typename TComponent>
        requires std::derived_from<TComponent, Component>
    bool EntityRegistry::has(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        auto handle = static_cast<entt::entity>(id.value - 1U);
        if (!_registry->valid(handle))
            return false;

        return _registry->all_of<TComponent>(handle);
    }
}
