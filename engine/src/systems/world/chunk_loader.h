#pragma once
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/world/manager.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/sphere.h"
#include "tbx/types/vectors.h"
#include <algorithm>
#include <limits>
#include <optional>
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
        // World-space bounds of the chunk's contents, computed the first time it loads and kept across
        // unloads so the view-based streamer can decide to re-load it without first deserializing it.
        // (Chunk coords are logical ids, not spatial cells, so bounds must come from the entities.)
        Sphere bounds = {};
        bool has_bounds = false;
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

    // World-space bounding sphere of a chunk's entities (AABB of their world positions, enclosed). A
    // coarse but synchronous bound for streaming — chunk coords are logical ids, not spatial cells, so
    // where a chunk actually sits has to come from its entities. Entities without a transform are
    // skipped; an empty chunk yields a zero-radius sphere at the origin.
    static Sphere compute_chunk_bounds(const EntityRegistry& entities)
    {
        auto minimum = Vec3(std::numeric_limits<float>::max());
        auto maximum = Vec3(std::numeric_limits<float>::lowest());
        auto any = false;
        for (const auto& entity : entities.get_all())
        {
            if (!entity.has_component<Transform>())
                continue;

            const auto position =
                entity.get_component<Transform>().to_world_space(entity).position;
            minimum = Vec3(
                std::min(minimum.x, position.x),
                std::min(minimum.y, position.y),
                std::min(minimum.z, position.z));
            maximum = Vec3(
                std::max(maximum.x, position.x),
                std::max(maximum.y, position.y),
                std::max(maximum.z, position.z));
            any = true;
        }

        if (!any)
            return Sphere {.center = Vec3(0.0F), .radius = 0.0F};

        return Sphere {
            .center = (minimum + maximum) * 0.5F,
            .radius = length(maximum - minimum) * 0.5F};
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

        // Coords of the chunks currently loaded into `world`.
        std::vector<IVec3> get_loaded_coords(const World& world) const
        {
            auto coords = std::vector<IVec3> {};
            const auto world_it = _worlds.find(world.id);
            if (world_it == _worlds.end())
                return coords;

            for (const auto& [coord, record] : world_it->second.chunks)
                if (record.is_loaded)
                    coords.push_back(coord);

            return coords;
        }

        bool is_chunk_loaded(const World& world, const IVec3& coord) const
        {
            const auto world_it = _worlds.find(world.id);
            if (world_it == _worlds.end())
                return false;

            const auto chunk_it = world_it->second.chunks.find(coord);
            return chunk_it != world_it->second.chunks.end() && chunk_it->second.is_loaded;
        }

        // The chunk asset handle to load for a coord (so the streaming lane can deserialize it off the
        // main thread); invalid when the coord is unknown.
        Handle get_chunk_handle(const World& world, const IVec3& coord) const
        {
            const auto world_it = _worlds.find(world.id);
            if (world_it == _worlds.end())
                return {};

            const auto chunk_it = world_it->second.chunks.find(coord);
            return chunk_it != world_it->second.chunks.end() ? chunk_it->second.full_chunk : Handle {};
        }

        // Integrate an already-loaded WorldChunk asset (the streaming lane did the deserialize): swap
        // its entities into the world and mark the chunk loaded. Main thread only (it mutates the ECS).
        void integrate_loaded(
            World& world,
            const IVec3& coord,
            const std::shared_ptr<WorldChunk>& chunk)
        {
            if (!chunk)
                return;

            auto world_it = _worlds.find(world.id);
            if (world_it == _worlds.end())
                return;

            auto chunk_it = world_it->second.chunks.find(coord);
            if (chunk_it == world_it->second.chunks.end() || chunk_it->second.is_loaded)
                return;

            TBX_TRACE_INFO("Loading world chunk ({}, {}, {})", coord.x, coord.y, coord.z);

            auto& record = chunk_it->second;
            world.remove_entities(record.entities);
            world.add_entities(chunk->entities);
            record.entities = collect_entity_ids(chunk->entities);
            record.is_loaded = true;
            if (!record.has_bounds)
            {
                record.bounds = compute_chunk_bounds(chunk->entities);
                record.has_bounds = true;
            }
        }

        // The chunk's cached world bounds, or nullopt if it has never been loaded (so its real extent
        // isn't known yet). Used by view-based streaming to decide whether a chunk is in view.
        std::optional<Sphere> get_chunk_bounds(const World& world, const IVec3& coord) const
        {
            const auto world_it = _worlds.find(world.id);
            if (world_it == _worlds.end())
                return std::nullopt;

            const auto chunk_it = world_it->second.chunks.find(coord);
            if (chunk_it == world_it->second.chunks.end() || !chunk_it->second.has_bounds)
                return std::nullopt;

            return chunk_it->second.bounds;
        }

        // Stream a chunk out: remove its entities and mark it unloaded. Main thread only.
        void unload_chunk(World& world, const IVec3& coord)
        {
            auto world_it = _worlds.find(world.id);
            if (world_it == _worlds.end())
                return;

            auto chunk_it = world_it->second.chunks.find(coord);
            if (chunk_it == world_it->second.chunks.end() || !chunk_it->second.is_loaded)
                return;

            TBX_TRACE_INFO("Unloading world chunk ({}, {}, {})", coord.x, coord.y, coord.z);

            world.remove_entities(chunk_it->second.entities);
            chunk_it->second.entities.clear();
            chunk_it->second.is_loaded = false;
        }

      private:
        std::weak_ptr<AssetManager> _asset_manager = {};
        std::unordered_map<Uuid, ChunkLoaderWorldRecord> _worlds = {};
    };
}
