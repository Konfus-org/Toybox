#include "tbx/systems/assets/manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/streamer.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/transform.h"

namespace tbx
{
    struct EntityStreamerCameraChunk
    {
        WorldChunkCoord coord = {};
    };

    struct EntityStreamerChunkState
    {
        bool is_loaded = false;
        WorldChunkLod visual_lod = WorldChunkLod::FULL;
        WorldSimulationMode simulation_mode = WorldSimulationMode::FROZEN;
    };

    struct EntityStreamerDesiredChunkState
    {
        bool should_load = false;
        WorldChunkLod visual_lod = WorldChunkLod::LOW;
        WorldSimulationMode simulation_mode = WorldSimulationMode::FROZEN;
    };

    struct EntityStreamerWorldState
    {
        std::unordered_map<WorldChunkCoord, EntityStreamerChunkState> chunks = {};
    };

    struct EntityStreamerState
    {
        std::weak_ptr<AssetManager> asset_manager = {};
        std::unordered_map<Uuid, EntityStreamerWorldState> world_states = {};
    };

    static int32 chunk_distance(const WorldChunkCoord& a, const WorldChunkCoord& b)
    {
        return std::max({std::abs(a.x - b.x), std::abs(a.y - b.y), std::abs(a.z - b.z)});
    }

    static WorldChunkCoord position_to_chunk(const Vec3& position, float chunk_size)
    {
        const float safe_chunk_size = std::max(chunk_size, 1.0F);
        return WorldChunkCoord(
            static_cast<int32>(std::floor(position.x / safe_chunk_size)),
            static_cast<int32>(std::floor(position.y / safe_chunk_size)),
            static_cast<int32>(std::floor(position.z / safe_chunk_size)));
    }

    static std::vector<EntityStreamerCameraChunk> collect_camera_chunks(World& world)
    {
        auto camera_chunks = std::vector<EntityStreamerCameraChunk> {};
        for (auto& camera_entity : world.get_with<Camera>())
        {
            auto position = Vec3(0.0F, 0.0F, 0.0F);
            if (camera_entity.has_component<Transform>())
                position = get_world_space_transform(camera_entity).position;

            camera_chunks.push_back(
                EntityStreamerCameraChunk {
                    .coord = position_to_chunk(position, world.chunk_size),
                });
        }

        return camera_chunks;
    }

    static EntityStreamerDesiredChunkState get_desired_chunk_state(
        const World& world,
        const WorldChunkCoord& coord,
        const std::vector<EntityStreamerCameraChunk>& camera_chunks)
    {
        if (camera_chunks.empty())
            return {};

        auto nearest_distance = std::numeric_limits<int32>::max();
        for (const auto& camera : camera_chunks)
            nearest_distance = std::min(nearest_distance, chunk_distance(coord, camera.coord));

        const auto distance = static_cast<uint32>(std::max(nearest_distance, 0));
        if (distance > world.unload_radius_chunks)
            return {};

        auto desired = EntityStreamerDesiredChunkState {
            .should_load = true,
            .visual_lod = distance <= world.full_visual_radius_chunks ? WorldChunkLod::FULL
                                                                      : WorldChunkLod::LOW,
            .simulation_mode = WorldSimulationMode::FROZEN,
        };

        if (distance <= world.simulation_radius_chunks)
            desired.simulation_mode = WorldSimulationMode::FULL;
        else if (distance <= world.reduced_simulation_radius_chunks)
            desired.simulation_mode = WorldSimulationMode::REDUCED;

        return desired;
    }

    static Handle select_chunk_handle(
        const WorldChunkRef& chunk_ref,
        const EntityStreamerDesiredChunkState& desired)
    {
        if (desired.simulation_mode != WorldSimulationMode::FROZEN
            && chunk_ref.simulation_chunk.is_valid())
            return chunk_ref.simulation_chunk;

        if (desired.visual_lod == WorldChunkLod::LOW && chunk_ref.low_lod_chunk.is_valid())
            return chunk_ref.low_lod_chunk;

        return chunk_ref.full_chunk;
    }

    struct EntityStreamer::State
    {
        EntityStreamerState data = {};
    };

    EntityStreamer::EntityStreamer(std::weak_ptr<AssetManager> asset_manager)
        : _state(std::make_unique<State>())
    {
        _state->data.asset_manager = std::move(asset_manager);
    }

    EntityStreamer::~EntityStreamer() noexcept = default;

    void EntityStreamer::update(const DeltaTime&)
    {
        const auto asset_manager = _state->data.asset_manager.lock();
        if (!asset_manager)
            return;

        for (const auto& world : asset_manager->get_loaded<World>())
        {
            if (world)
                update_world(*asset_manager, *world);
        }
    }

    void EntityStreamer::update_world(AssetManager& asset_manager, World& world)
    {
        const auto camera_chunks = collect_camera_chunks(world);
        auto& world_state = _state->data.world_states[world.id];
        auto touched_chunks = std::unordered_map<WorldChunkCoord, bool> {};

        for (const auto& chunk_ref : world.chunks)
        {
            const auto desired = get_desired_chunk_state(world, chunk_ref.coord, camera_chunks);
            touched_chunks[chunk_ref.coord] = desired.should_load;

            auto& chunk_state = world_state.chunks[chunk_ref.coord];
            if (!desired.should_load)
            {
                if (chunk_state.is_loaded)
                    world.unload_chunk(chunk_ref.coord);
                chunk_state = {};
                continue;
            }

            const bool needs_reload =
                !chunk_state.is_loaded || chunk_state.visual_lod != desired.visual_lod;
            if (needs_reload)
            {
                const Handle chunk_handle = select_chunk_handle(chunk_ref, desired);
                auto chunk = asset_manager.load<WorldChunk>(chunk_handle);
                if (!chunk)
                    continue;

                TBX_TRACE_INFO(
                    "Loading world chunk ({}, {}, {}) with lod {}",
                    chunk->coord.x,
                    chunk->coord.y,
                    chunk->coord.z,
                    static_cast<int>(desired.visual_lod));

                world.load_chunk(*chunk, desired.simulation_mode);
                chunk_state.is_loaded = true;
                chunk_state.visual_lod = desired.visual_lod;
                chunk_state.simulation_mode = desired.simulation_mode;

                continue;
            }

            if (chunk_state.simulation_mode != desired.simulation_mode)
            {
                world.set_chunk_simulation_mode(chunk_ref.coord, desired.simulation_mode);
                chunk_state.simulation_mode = desired.simulation_mode;
            }
        }

        auto stale_chunks = std::vector<WorldChunkCoord> {};
        for (const auto& chunk_entry : world_state.chunks)
        {
            if (touched_chunks.contains(chunk_entry.first))
                continue;

            stale_chunks.push_back(chunk_entry.first);
        }

        for (const auto& coord : stale_chunks)
        {
            TBX_TRACE_INFO("Unloading world chunk ({}, {}, {})", coord.x, coord.y, coord.z);

            world.unload_chunk(coord);
            world_state.chunks.erase(coord);
        }
    }
}
