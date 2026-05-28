#include "systems/ecs/internal/streamer_internal.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/streamer.h"
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tbx
{
    struct EntityStreamer::Impl
    {
        internal::EntityStreamerState state = {};
    };

    EntityStreamer::EntityStreamer(std::weak_ptr<AssetManager> asset_manager)
        : _impl(std::make_unique<Impl>())
    {
        _impl->state.asset_manager = std::move(asset_manager);
    }

    EntityStreamer::~EntityStreamer() noexcept = default;

    void EntityStreamer::update(const DeltaTime&)
    {
        const auto asset_manager = _impl->state.asset_manager.lock();
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
        const auto camera_chunks = internal::collect_camera_chunks(world);
        auto& world_state = _impl->state.world_states[world.id];
        auto touched_chunks = std::unordered_map<WorldChunkCoord, bool> {};

        for (const auto& chunk_ref : world.chunks)
        {
            const auto desired =
                internal::get_desired_chunk_state(world, chunk_ref.coord, camera_chunks);
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
                const Handle chunk_handle = internal::select_chunk_handle(chunk_ref, desired);
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
