#pragma once
#include "tbx/systems/world/manager.h"
#include "tbx/systems/world/settings.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/frustum.h"
#include "tbx/types/sphere.h"
#include "tbx/types/vectors.h"
#include <vector>

namespace tbx
{
    // One active camera's view this frame: the frustum to test chunks against and its world position
    // (for the always-loaded bubble). Snapshotted on the main thread; the streamer never touches the
    // World off the main thread.
    struct StreamerCameraView
    {
        Frustum frustum = {};
        Vec3 position = Vec3(0.0F);
    };

    // Decides which world chunks should be loaded from the camera's point of view, the SAME way the
    // renderer culls objects: a chunk is wanted when its world bounds are visible to a camera frustum
    // (so streaming reaches exactly as far as rendering does), or within a small keep-loaded bubble so
    // turning around / nearby shadow casters don't pop. It owns no state and never loads anything.
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
        // Snapshot every active camera's frustum + position (main thread: it reads the World).
        std::vector<StreamerCameraView> collect_views(World& world) const
        {
            auto views = std::vector<StreamerCameraView> {};
            for (auto& camera_entity : world.get_with<Camera>())
            {
                auto position = Vec3(0.0F);
                auto rotation = Quat(1.0F, 0.0F, 0.0F, 0.0F);
                if (camera_entity.has_component<Transform>())
                {
                    const auto transform =
                        camera_entity.get_component<Transform>().to_world_space(camera_entity);
                    position = transform.position;
                    rotation = transform.rotation;
                }
                const auto& camera = camera_entity.get_component<Camera>();
                views.push_back(
                    StreamerCameraView {
                        .frustum = camera.get_frustum(position, rotation),
                        .position = position,
                    });
            }
            return views;
        }

        // Whether a chunk with world bounds `bounds` should be loaded: visible to any camera (same
        // frustum/sphere test the renderer culls objects with), or within keep_radius of a camera.
        bool is_chunk_visible(
            const std::vector<StreamerCameraView>& views,
            const Sphere& bounds,
            float keep_radius) const
        {
            for (const auto& view : views)
            {
                if (keep_radius > 0.0F
                    && distance(bounds.center, view.position) <= keep_radius + bounds.radius)
                    return true;
                if (view.frustum.intersects(bounds))
                    return true;
            }
            return false;
        }
    };
}
