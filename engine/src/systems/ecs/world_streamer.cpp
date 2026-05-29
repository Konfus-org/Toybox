#include "tbx/systems/assets/manager.h"
#include "tbx/systems/app/settings.h"
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
    };

    struct EntityStreamerDesiredChunkState
    {
        bool should_load = false;
    };

    struct EntityStreamerWorldState
    {
        std::unordered_map<WorldChunkCoord, EntityStreamerChunkState> chunks = {};
    };

    struct EntityStreamerState
    {
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

    static std::vector<EntityStreamerCameraChunk> collect_camera_chunks(
        World& world,
        float chunk_size)
    {
        auto camera_chunks = std::vector<EntityStreamerCameraChunk> {};
        for (auto& camera_entity : world.get_with<Camera>())
        {
            auto position = Vec3(0.0F, 0.0F, 0.0F);
            if (camera_entity.has_component<Transform>())
                position = get_world_space_transform(camera_entity).position;

            camera_chunks.push_back(
                EntityStreamerCameraChunk {
                    .coord = position_to_chunk(position, chunk_size),
                });
        }

        return camera_chunks;
    }

    static EntityStreamerDesiredChunkState get_desired_chunk_state(
        const WorldChunkCoord& coord,
        uint32 unload_radius_chunks,
        const std::vector<EntityStreamerCameraChunk>& camera_chunks)
    {
        if (camera_chunks.empty())
            return {};

        auto nearest_distance = std::numeric_limits<int32>::max();
        for (const auto& camera : camera_chunks)
            nearest_distance = std::min(nearest_distance, chunk_distance(coord, camera.coord));

        const auto distance = static_cast<uint32>(std::max(nearest_distance, 0));
        if (distance > unload_radius_chunks)
            return {};

        return EntityStreamerDesiredChunkState {
            .should_load = true,
        };
    }

    struct EntityStreamer::State
    {
        EntityStreamerState data = {};
    };

    EntityStreamer::EntityStreamer(
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<AppSettings> settings)
        : _state(std::make_unique<State>())
        , _asset_manager(std::move(asset_manager))
    {
        if (const auto app_settings = settings.lock())
            _settings = app_settings->world.value;
    }

    EntityStreamer::~EntityStreamer() noexcept = default;

    void EntityStreamer::update(const DeltaTime&)
    {
        const auto asset_manager = _asset_manager.lock();
        if (!asset_manager)
            return;

        for (const auto& world : asset_manager->get_loaded<World>())
        {
            if (world)
            {
                update_world(
                    *asset_manager,
                    *world,
                    _settings.chunk_size.value,
                    _settings.unload_radius_chunks.value);
            }
        }
    }

    void EntityStreamer::receive_message(Message& msg)
    {
        const auto world_settings_event = handle_property_changed<&AppSettings::world>(msg);
        if (!world_settings_event)
            return;

        _settings = world_settings_event->get().current;
    }

    void EntityStreamer::update_world(
        AssetManager& asset_manager,
        World& world,
        float chunk_size,
        uint32 unload_radius_chunks)
    {
        const auto camera_chunks = collect_camera_chunks(world, chunk_size);
        auto& world_state = _state->data.world_states[world.id];
        auto touched_chunks = std::unordered_map<WorldChunkCoord, bool> {};

        for (const auto& chunk_ref : world.chunks)
        {
            const auto desired =
                get_desired_chunk_state(chunk_ref.coord, unload_radius_chunks, camera_chunks);
            touched_chunks[chunk_ref.coord] = desired.should_load;

            auto& chunk_state = world_state.chunks[chunk_ref.coord];
            if (!desired.should_load)
            {
                if (chunk_state.is_loaded)
                    world.unload_chunk(chunk_ref.coord);
                chunk_state = {};
                continue;
            }

            if (!chunk_state.is_loaded)
            {
                auto chunk = asset_manager.load<WorldChunk>(chunk_ref.full_chunk);
                if (!chunk)
                    continue;

                TBX_TRACE_INFO(
                    "Loading world chunk ({}, {}, {})",
                    chunk->coord.x,
                    chunk->coord.y,
                    chunk->coord.z);

                world.load_chunk(*chunk);
                chunk_state.is_loaded = true;
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
