#include "tbx/types/assets/world.h"
#include "tbx/systems/debugging/macros.h"
#include <algorithm>

namespace tbx
{
    static WorldChunkCoord make_chunk_coord(const Vec3& position, float chunk_size)
    {
        const float safe_chunk_size = std::max(chunk_size, 1.0F);
        return WorldChunkCoord(
            static_cast<int32>(std::floor(position.x / safe_chunk_size)),
            static_cast<int32>(std::floor(position.y / safe_chunk_size)),
            static_cast<int32>(std::floor(position.z / safe_chunk_size)));
    }

    World::World() = default;

    World::~World() noexcept = default;

    Entity World::create_entity(const std::string& name)
    {
        return create_entity(name, Uuid(), false, 32.0F);
    }

    Entity World::create_entity(const std::string& name, const Uuid& parent)
    {
        return create_entity(name, parent, false, 32.0F);
    }

    Entity World::create_global_entity(const std::string& name)
    {
        // TODO: Take into account world settings!
        return create_entity(name, Uuid(), true, 32.0F);
    }

    void World::destroy(Entity& entity)
    {
        const Uuid id = entity.get_id();
        remove_entity_from_chunk_tracking(id);
        remove_global(id);
        _registry.remove(entity);
    }

    void World::clear_runtime_entities()
    {
        _registry.clear();
        _loaded_entities_by_chunk.clear();
        _chunk_by_entity.clear();
        _entities_by_chunk.clear();
    }

    void World::rebuild_global_entities()
    {
        auto entity_records = std::vector<std::string> {};
        entity_records.reserve(globals.size());
        for (const auto& entity_record : globals)
            entity_records.push_back(Entity::serialize(entity_record));

        clear_runtime_entities();
        globals.clear();
        globals.reserve(entity_records.size());

        // Serialized entity records deserialize independently. Rebind them into this world's
        // registry so parent lookups and runtime component mutations share one ECS context.
        for (const auto& entity_record : entity_records)
        {
            auto entity = Entity();
            if (!Entity::deserialize(entity_record, _registry, entity))
                continue;

            const auto id = entity.get_id();
            if (!id.is_valid())
                continue;

            globals.push_back(entity);
        }
    }

    void World::update_chunk_membership(float chunk_size)
    {
        auto entities = _registry.get_all();
        for (auto& entity : entities)
        {
            const Uuid id = entity.get_id();
            if (has_global(id))
                continue;

            if (!entity.has_component<Transform>())
            {
                make_persistent(id);
                continue;
            }

            assign_entity_to_chunk(entity, chunk_size);
        }
    }

    bool World::has(const Uuid& id) const
    {
        if (_registry.has(id))
            return true;

        if (_chunk_by_entity.contains(id))
            return true;

        return std::ranges::any_of(
            globals,
            [&id](const Entity& entity)
            {
                return entity.get_id() == id;
            });
    }

    bool World::is_global(const Uuid& id) const
    {
        return has_global(id);
    }

    bool World::try_get_chunk(const Uuid& id, WorldChunkCoord& out_coord) const
    {
        const auto chunk_it = _chunk_by_entity.find(id);
        if (chunk_it == _chunk_by_entity.end())
            return false;

        out_coord = chunk_it->second;
        return true;
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
        auto entity = _registry.get(id);
        if (entity.get_id().is_valid())
            return entity;

        const auto chunk_it = _chunk_by_entity.find(id);
        if (chunk_it != _chunk_by_entity.end())
        {
            const auto loaded_it = _loaded_entities_by_chunk.find(chunk_it->second);
            if (loaded_it != _loaded_entities_by_chunk.end())
            {
                const auto entity_it = std::ranges::find_if(
                    loaded_it->second,
                    [&id](const Entity& loaded_entity)
                    {
                        return loaded_entity.get_id() == id;
                    });
                if (entity_it != loaded_it->second.end())
                    return *entity_it;
            }
        }

        const auto persistent_it = std::ranges::find_if(
            globals,
            [&id](const Entity& persistent_entity)
            {
                return persistent_entity.get_id() == id;
            });
        return persistent_it != globals.end() ? *persistent_it : Entity();
    }

    std::vector<Entity> World::get_all() const
    {
        auto entities = _registry.get_all();
        for (const auto& persistent_entity : globals)
        {
            if (!_registry.has(persistent_entity.get_id()))
                entities.push_back(persistent_entity);
        }
        for (const auto& chunk_entry : _loaded_entities_by_chunk)
        {
            const auto& chunk_entities = chunk_entry.second;
            entities.insert(entities.end(), chunk_entities.begin(), chunk_entities.end());
        }
        return entities;
    }

    void World::load_chunk(const WorldChunk& chunk)
    {
        unload_chunk(chunk.coord);
        auto& loaded_entities = _loaded_entities_by_chunk[chunk.coord];
        loaded_entities.reserve(chunk.entities.size());
        for (const auto& entity_record : chunk.entities)
        {
            auto entity = entity_record;
            const auto id = entity.get_id();
            if (!id.is_valid())
                continue;

            loaded_entities.push_back(entity);
            _chunk_by_entity[id] = chunk.coord;
            _entities_by_chunk[chunk.coord].push_back(id);
        }
    }

    void World::unload_chunk(const WorldChunkCoord& coord)
    {
        auto entities_it = _entities_by_chunk.find(coord);
        if (entities_it == _entities_by_chunk.end())
            return;

        auto entity_ids = entities_it->second;
        for (const auto& id : entity_ids)
            _chunk_by_entity.erase(id);

        _loaded_entities_by_chunk.erase(coord);
        _entities_by_chunk.erase(coord);
    }

    Entity World::create_entity(
        const std::string& name,
        const Uuid& parent,
        bool is_persistent,
        float chunk_size)
    {
        const auto id = _registry.add(name, "", "", parent);
        auto entity = _registry.get(id);
        if (is_persistent)
        {
            make_persistent(id);
            return entity;
        }

        assign_entity_to_chunk(entity, chunk_size);
        return entity;
    }

    void World::assign_entity_to_chunk(const Entity& entity, float chunk_size)
    {
        if (!entity.get_id().is_valid())
            return;

        auto coord = WorldChunkCoord {};
        if (entity.has_component<Transform>())
        {
            const auto& transform = entity.get_component<Transform>();
            coord = make_chunk_coord(transform.position, chunk_size);
        }

        const Uuid id = entity.get_id();
        const auto existing_it = _chunk_by_entity.find(id);
        if (existing_it != _chunk_by_entity.end() && existing_it->second == coord)
            return;

        remove_entity_from_chunk_tracking(id);
        remove_global(id);
        _chunk_by_entity[id] = coord;
        _entities_by_chunk[coord].push_back(id);
    }

    bool World::has_global(const Uuid& id) const
    {
        return std::ranges::any_of(
            globals,
            [&id](const Entity& entity)
            {
                return entity.get_id() == id;
            });
    }

    void World::make_persistent(const Uuid& id)
    {
        remove_entity_from_chunk_tracking(id);
        if (has_global(id))
            return;

        auto entity = _registry.get(id);
        if (entity.get_id().is_valid())
            globals.push_back(entity);
    }

    void World::remove_global(const Uuid& id)
    {
        globals.erase(
            std::remove_if(
                globals.begin(),
                globals.end(),
                [&id](const Entity& entity)
                {
                    return entity.get_id() == id;
                }),
            globals.end());
    }

    void World::remove_entity_from_chunk_tracking(const Uuid& id)
    {
        const auto existing_it = _chunk_by_entity.find(id);
        if (existing_it == _chunk_by_entity.end())
            return;

        const auto coord = existing_it->second;
        _chunk_by_entity.erase(existing_it);

        auto entities_it = _entities_by_chunk.find(coord);
        if (entities_it == _entities_by_chunk.end())
            return;

        auto& ids = entities_it->second;
        ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
        if (ids.empty())
            _entities_by_chunk.erase(entities_it);
    }
}
