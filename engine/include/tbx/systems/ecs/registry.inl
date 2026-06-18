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

        std::vector<Entity> entities = {};
        entities.reserve(ids.size());
        for (const auto& id : ids)
        {
            // Disabled entities are turned off wholesale: skip them so no runtime system that gathers
            // through this query (rendering, physics, scripting) ever touches them. The editor lists them
            // via the unfiltered get_all / serialize path instead. Checked outside the view lock above.
            if (!get_enabled(id))
                continue;

            entities.push_back(get(id));
        }

        return entities;
    }

    template <typename... TComponent>
        requires(std::derived_from<TComponent, Component> && ...)
    Entity EntityRegistry::first_with() const
    {
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

        // The first *enabled* match: a disabled entity is turned off wholesale, so it can't be the one a
        // runtime system (e.g. the sky / post-processing / camera lookups) acts on. Checked outside the lock.
        for (const auto& id : ids)
        {
            if (get_enabled(id))
                return get(id);
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
