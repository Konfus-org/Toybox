#pragma once
#include "tbx/systems/world/manager.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/transform.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace tbx
{
    struct EntityStreamerCameraChunk
    {
        IVec3 coord = {};
    };

    static int32 chunk_distance(const IVec3& a, const IVec3& b)
    {
        return std::max({std::abs(a.x - b.x), std::abs(a.y - b.y), std::abs(a.z - b.z)});
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
                position =
                    camera_entity.get_component<Transform>().to_world_space(camera_entity).position;

            const float safe_chunk_size = std::max(chunk_size, 1.0F);
            camera_chunks.push_back(
                EntityStreamerCameraChunk {
                    .coord = IVec3(
                        static_cast<int32>(std::floor(position.x / safe_chunk_size)),
                        static_cast<int32>(std::floor(position.y / safe_chunk_size)),
                        static_cast<int32>(std::floor(position.z / safe_chunk_size))),
                });
        }

        return camera_chunks;
    }

    static bool should_load_chunk(
        const IVec3& coord,
        uint32 unload_radius_chunks,
        const std::vector<EntityStreamerCameraChunk>& camera_chunks)
    {
        if (camera_chunks.empty())
            return false;

        auto nearest_distance = std::numeric_limits<int32>::max();
        for (const auto& camera : camera_chunks)
            nearest_distance = std::min(nearest_distance, chunk_distance(coord, camera.coord));

        const auto distance = static_cast<uint32>(std::max(nearest_distance, 0));
        return distance <= unload_radius_chunks;
    }

    class EntityStreamer final
    {
      public:
        EntityStreamer() = default;
        ~EntityStreamer() noexcept = default;

      public:
        EntityStreamer(const EntityStreamer&) = delete;
        EntityStreamer& operator=(const EntityStreamer&) = delete;
        EntityStreamer(EntityStreamer&&) noexcept = delete;
        EntityStreamer& operator=(EntityStreamer&&) noexcept = delete;

      public:
        std::vector<IVec3> get_desired_chunks(
            World& world,
            const std::vector<IVec3>& chunk_coords,
            const DeltaTime&,
            const WorldSettings& settings) const
        {
            const auto camera_chunks = collect_camera_chunks(world, settings.chunk_size);
            auto desired_chunks = std::vector<IVec3> {};
            desired_chunks.reserve(chunk_coords.size());

            for (const auto& coord : chunk_coords)
            {
                if (should_load_chunk(coord, settings.unload_radius_chunks, camera_chunks))
                {
                    desired_chunks.push_back(coord);
                }
            }

            return desired_chunks;
        }
    };
}
