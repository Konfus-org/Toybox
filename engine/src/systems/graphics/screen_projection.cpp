#include "tbx/systems/graphics/screen_projection.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/vectors.h"

namespace tbx
{
    std::vector<EntityScreenPosition> project_entities_to_screen(
        const CameraView& camera,
        World& world,
        const std::unordered_set<Uuid>* only)
    {
        // Project through one precomputed view-projection — CameraView::project_to_screen would rebuild
        // it per entity, which is wasteful when sweeping a whole world.
        const auto view_projection =
            camera.camera.get_view_projection_matrix(camera.position, camera.rotation);

        auto positions = std::vector<EntityScreenPosition>();
        for (auto entity : world.get_with<Transform>())
        {
            if (only != nullptr && !only->contains(entity.get_id()))
                continue;

            // Transient entities (a host's injected view cameras) are plumbing, not world content —
            // never worth a screen anchor.
            if (!entity.is_serialized())
                continue;

            const auto world_position =
                entity.get_component<Transform>().to_world_space(entity).position;

            auto u = 0.0F;
            auto v = 0.0F;
            if (!project_to_screen(view_projection, world_position, u, v))
                continue; // behind the camera

            positions.push_back(
                EntityScreenPosition {
                    .id = entity.get_id(),
                    .u = u,
                    .v = v,
                    .depth = distance(camera.position, world_position),
                });
        }
        return positions;
    }
}
