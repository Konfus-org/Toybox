#pragma once
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/world/manager.h"
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tbx
{
    struct WorldChunkRef
    {
        IVec3 coord = {};
        Handle full_chunk = {};
    };

    struct ChunkLoaderChunkRecord
    {
        Handle full_chunk = {};
        bool is_loaded = false;
        std::vector<Uuid> entities = {};
    };

    struct ChunkLoaderWorldRecord
    {
        std::unordered_map<IVec3, ChunkLoaderChunkRecord> chunks = {};
    };

    static std::vector<Uuid> collect_entity_ids(const std::vector<Entity>& entities)
    {
        auto ids = std::vector<Uuid> {};
        ids.reserve(entities.size());
        for (const auto& entity : entities)
        {
            if (entity.get_id().is_valid())
                ids.push_back(entity.get_id());
        }

        return ids;
    }

    class ChunkLoader final
    {
      public:
        ChunkLoader(std::weak_ptr<AssetManager> asset_manager)
            : _asset_manager(std::move(asset_manager))
        {
        }

        ~ChunkLoader() noexcept = default;

      public:
        ChunkLoader(const ChunkLoader&) = delete;
        ChunkLoader& operator=(const ChunkLoader&) = delete;
        ChunkLoader(ChunkLoader&&) noexcept = delete;
        ChunkLoader& operator=(ChunkLoader&&) noexcept = delete;

      public:
        void clear(World& world)
        {
            auto world_it = _worlds.find(world.id);
            if (world_it == _worlds.end())
                return;

            for (const auto& chunk_entry : world_it->second.chunks)
                world.remove_entities(chunk_entry.second.entities);

            _worlds.erase(world_it);
        }

        std::vector<IVec3> get_chunk_coords(const World& world) const
        {
            auto coords = std::vector<IVec3> {};
            const auto world_it = _worlds.find(world.id);
            if (world_it == _worlds.end())
                return coords;

            coords.reserve(world_it->second.chunks.size());
            for (const auto& chunk_entry : world_it->second.chunks)
                coords.push_back(chunk_entry.first);

            return coords;
        }

        bool set_chunks(World& world)
        {
            const auto asset_manager = _asset_manager.lock();
            if (!asset_manager)
                return world.chunks.empty();

            auto world_record = ChunkLoaderWorldRecord {};
            for (const auto& chunk_handle : world.chunks)
            {
                auto chunk = asset_manager->load<WorldChunk>(chunk_handle);
                if (!chunk)
                    return false;

                world_record.chunks[chunk->coord].full_chunk = chunk_handle;
            }

            clear(world);
            _worlds[world.id] = std::move(world_record);
            return true;
        }

        void update(World& world, const std::vector<IVec3>& desired_chunks)
        {
            const auto asset_manager = _asset_manager.lock();
            if (!asset_manager)
                return;

            auto& world_state = _worlds[world.id];

            auto touched_chunks = std::unordered_set<IVec3> {};
            for (const auto& coord : desired_chunks)
            {
                auto chunk_state_it = world_state.chunks.find(coord);
                if (chunk_state_it == world_state.chunks.end())
                    continue;

                touched_chunks.insert(coord);
                auto& chunk_state = chunk_state_it->second;
                if (chunk_state.is_loaded)
                    continue;

                auto chunk = asset_manager->load<WorldChunk>(chunk_state.full_chunk);
                if (!chunk)
                    continue;

                TBX_TRACE_INFO(
                    "Loading world chunk ({}, {}, {})",
                    chunk->coord.x,
                    chunk->coord.y,
                    chunk->coord.z);

                world.add_entities(chunk->entities);
                chunk_state.entities = collect_entity_ids(chunk->entities);
                chunk_state.is_loaded = true;
            }

            auto chunks_to_unload = std::vector<IVec3> {};
            for (const auto& chunk_entry : world_state.chunks)
            {
                if (!touched_chunks.contains(chunk_entry.first))
                    chunks_to_unload.push_back(chunk_entry.first);
            }

            for (const auto& coord : chunks_to_unload)
            {
                auto chunk_it = world_state.chunks.find(coord);
                if (chunk_it == world_state.chunks.end())
                    continue;

                TBX_TRACE_INFO("Unloading world chunk ({}, {}, {})", coord.x, coord.y, coord.z);

                world.remove_entities(chunk_it->second.entities);
                world_state.chunks.erase(chunk_it);
            }
        }

      private:
        std::weak_ptr<AssetManager> _asset_manager = {};
        std::unordered_map<Uuid, ChunkLoaderWorldRecord> _worlds = {};
    };
}
