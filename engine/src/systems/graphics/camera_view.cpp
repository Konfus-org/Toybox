#include "tbx/systems/graphics/camera_view.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/types/components/transform.h"

namespace tbx
{
    CameraView CameraView::from_entity(Entity camera_entity)
    {
        auto view = CameraView();
        if (!camera_entity.get_id().is_valid() || !camera_entity.has_component<Camera>()
            || !camera_entity.has_component<Transform>())
            return view;

        const auto camera_world =
            camera_entity.get_component<Transform>().to_world_space(camera_entity);
        view.camera = camera_entity.get_component<Camera>();
        view.position = camera_world.position;
        view.rotation = camera_world.rotation;
        view.is_valid = true;
        return view;
    }
}
