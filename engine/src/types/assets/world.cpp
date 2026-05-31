#include "tbx/types/assets/world.h"
#include <algorithm>

namespace tbx
{
    World::World() = default;

    World::~World() noexcept = default;

    Entity World::create_entity(const std::string& name)
    {
        const auto id = _registry.add(name);
        return _registry.get(id);
    }

    Entity World::create_entity(const std::string& name, const Uuid& parent)
    {
        const auto id = _registry.add(name, "", "", parent);
        return _registry.get(id);
    }

    Entity World::create_global_entity(const std::string& name)
    {
        auto entity = create_entity(name);
        make_persistent(entity.get_id());
        return entity;
    }

    void World::destroy(Entity& entity)
    {
        const Uuid id = entity.get_id();
        remove_global(id);
        _registry.remove(entity);
    }

    void World::clear_runtime_entities()
    {
        _registry.clear();
        _global_entities.clear();
    }

    void World::add_entities(const std::vector<Entity>& entities)
    {
        for (const auto& source : entities)
        {
            auto entity = Entity();
            const auto entity_record = Entity::serialize(source);
            if (!Entity::deserialize(entity_record, _registry, entity))
                continue;

            if (entity.get_id().is_valid())
                remove_global(entity.get_id());
        }
    }

    void World::load_globals(const WorldGlobals& globals_asset)
    {
        for (const auto& source : globals_asset.entities)
        {
            auto entity = Entity();
            const auto entity_record = Entity::serialize(source);
            if (!Entity::deserialize(entity_record, _registry, entity))
                continue;

            make_persistent(entity.get_id());
        }
    }

    void World::remove_entities(const std::vector<Uuid>& ids)
    {
        for (const auto& id : ids)
        {
            if (!id.is_valid())
                continue;

            auto entity = _registry.get(id);
            if (entity.get_id().is_valid())
                destroy(entity);
        }
    }

    bool World::has(const Uuid& id) const
    {
        return _registry.has(id);
    }

    bool World::is_global(const Uuid& id) const
    {
        return has_global(id);
    }

    Entity World::find_by_name(std::string_view name) const
    {
        for (const auto& entity : get_all())
        {
            if (entity.get_name() == name)
                return entity;
        }

        return {};
    }

    Entity World::find_by_tag(std::string_view tag) const
    {
        for (const auto& entity : get_all())
        {
            if (entity.get_tag() == tag)
                return entity;
        }

        return {};
    }

    Entity World::get(const Uuid& id) const
    {
        return _registry.get(id);
    }

    std::vector<Entity> World::get_all() const
    {
        return _registry.get_all();
    }

    bool World::has_global(const Uuid& id) const
    {
        return _global_entities.contains(id);
    }

    void World::make_persistent(const Uuid& id)
    {
        if (!id.is_valid())
            return;

        _global_entities.insert(id);
    }

    void World::remove_global(const Uuid& id)
    {
        _global_entities.erase(id);
    }
}
