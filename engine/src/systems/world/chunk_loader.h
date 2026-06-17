#pragma once
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/world/manager.h"
#include <ranges>
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

    // A loaded chunk's identity plus the ids of the entities it currently owns — what saving needs to write
    // each chunk's entities back to its own asset file.
    struct LoadedChunkInfo
    {
        IVec3 coord = {};
        Handle handle = {};
        std::vector<Uuid> entities = {};
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

    static std::vector<Uuid> collect_entity_ids(const EntityRegistry& entities)
    {
        auto ids = std::vector<Uuid> {};
        for (const auto& entity : entities.get_all())
        {
            if (entity.get_id().is_valid())
                ids.push_back(entity.get_id());
        }

        return ids;
    }

    static bool handles_match(const Handle& left, const Handle& right)
    {
        if (left.id.is_valid() && right.id.is_valid())
            return left.id == right.id;

        return !left.name.empty() && left.name == right.name;
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

        std::vector<Uuid> get_loaded_chunk_entity_ids(const World& world) const
        {
            auto ids = std::vector<Uuid> {};
            const auto world_it = _worlds.find(world.id);
            if (world_it == _worlds.end())
                return ids;

            for (const auto& chunk_entry : world_it->second.chunks)
            {
                ids.insert(
                    ids.end(),
                    chunk_entry.second.entities.begin(),
                    chunk_entry.second.entities.end());
            }

            return ids;
        }

        // The loaded chunks of a world (coord + handle + owned entity ids) — used by saving to write each
        // chunk's current entities back to its own asset file.
        std::vector<LoadedChunkInfo> get_loaded_chunks(const World& world) const
        {
            auto result = std::vector<LoadedChunkInfo> {};
            const auto world_it = _worlds.find(world.id);
            if (world_it == _worlds.end())
                return result;

            for (const auto& [coord, record] : world_it->second.chunks)
            {
                if (record.is_loaded)
                    result.push_back(LoadedChunkInfo { coord, record.full_chunk, record.entities });
            }

            return result;
        }

        std::vector<Handle> get_loaded_chunk_handles(const World& world) const
        {
            auto handles = std::vector<Handle> {};
            const auto world_it = _worlds.find(world.id);
            if (world_it == _worlds.end())
                return handles;

            for (const auto& chunk_entry : world_it->second.chunks)
            {
                if (chunk_entry.second.is_loaded)
                    handles.push_back(chunk_entry.second.full_chunk);
            }

            return handles;
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

        bool load_chunks(World& world, const std::vector<Handle>& chunk_handles)
        {
            bool loaded_any = false;
            for (const auto& handle : chunk_handles)
                loaded_any = refresh_loaded_chunk(world, handle, true) || loaded_any;

            return loaded_any;
        }

        bool refresh_loaded_chunk(
            World& world,
            const Handle& chunk_handle,
            bool load_when_unloaded = false)
        {
            const auto asset_manager = _asset_manager.lock();
            if (!asset_manager)
                return false;

            auto world_it = _worlds.find(world.id);
            if (world_it == _worlds.end())
                return false;

            auto chunk_it = std::ranges::find_if(
                world_it->second.chunks,
                [&chunk_handle](const auto& entry)
                {
                    return handles_match(entry.second.full_chunk, chunk_handle);
                });
            if (chunk_it == world_it->second.chunks.end())
                return false;

            auto chunk_record = chunk_it->second;
            if (!chunk_record.is_loaded && !load_when_unloaded)
                return false;

            auto chunk = asset_manager->load<WorldChunk>(chunk_record.full_chunk);
            if (!chunk)
                return false;

            world.remove_entities(chunk_record.entities);
            chunk_record.entities.clear();
            chunk_record.is_loaded = false;
            world.add_entities(chunk->entities);
            chunk_record.entities = collect_entity_ids(chunk->entities);
            chunk_record.is_loaded = true;

            if (chunk->coord == chunk_it->first)
            {
                chunk_it->second = std::move(chunk_record);
                return true;
            }

            world_it->second.chunks.erase(chunk_it);
            world_it->second.chunks[chunk->coord] = std::move(chunk_record);
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
                chunk_it->second.entities.clear();
                chunk_it->second.is_loaded = false;
            }
        }

      private:
        std::weak_ptr<AssetManager> _asset_manager = {};
        std::unordered_map<Uuid, ChunkLoaderWorldRecord> _worlds = {};
    };
}
