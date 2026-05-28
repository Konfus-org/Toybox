#pragma once

namespace tbx
{
    template <typename TComponent>
        requires std::derived_from<TComponent, Component>
    bool World::has(const Uuid& id) const
    {
        if (_registry.has<TComponent>(id))
            return true;

        const auto entity = get(id);
        return entity.get_id().is_valid() && entity.has_component<TComponent>();
    }

    template <typename... TComponent>
        requires(std::derived_from<TComponent, Component> && ...)
    std::vector<Entity> World::get_with() const
    {
        auto entities = _registry.get_with<TComponent...>();
        for (const auto& entity : persistent_entities)
        {
            if (_registry.has(entity.get_id()))
                continue;

            if ((entity.has_component<TComponent>() && ...))
                entities.push_back(entity);
        }
        for (const auto& chunk_entry : _loaded_entities_by_chunk)
        {
            for (const auto& entity : chunk_entry.second)
            {
                if ((entity.has_component<TComponent>() && ...))
                    entities.push_back(entity);
            }
        }

        return entities;
    }

    template <typename... TComponent>
        requires(std::derived_from<TComponent, Component> && ...)
    Entity World::first_with() const
    {
        auto entity = _registry.first_with<TComponent...>();
        if (entity.get_id().is_valid())
            return entity;

        for (const auto& persistent_entity : persistent_entities)
        {
            if (_registry.has(persistent_entity.get_id()))
                continue;

            if ((persistent_entity.has_component<TComponent>() && ...))
                return persistent_entity;
        }
        for (const auto& chunk_entry : _loaded_entities_by_chunk)
        {
            for (const auto& chunk_entity : chunk_entry.second)
            {
                if ((chunk_entity.has_component<TComponent>() && ...))
                    return chunk_entity;
            }
        }

        return {};
    }

    template <typename... TComponent>
        requires(std::derived_from<TComponent, Component> && ...)
    void World::for_each_with(const std::function<void(Entity&)>& callback)
    {
        _registry.for_each_with<TComponent...>(callback);
        if (!callback)
            return;

        for (auto& entity : persistent_entities)
        {
            if (_registry.has(entity.get_id()))
                continue;

            if ((entity.has_component<TComponent>() && ...))
                callback(entity);
        }
        for (auto& chunk_entry : _loaded_entities_by_chunk)
        {
            for (auto& entity : chunk_entry.second)
            {
                if ((entity.has_component<TComponent>() && ...))
                    callback(entity);
            }
        }
    }
}
