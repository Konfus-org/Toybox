#include "tbx/systems/graphics/camera_view.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/ray.h"
#include "tbx/types/vectors.h"

namespace tbx
{
    bool project_to_screen(
        const Mat4& view_projection,
        const Vec3& world,
        float& out_u,
        float& out_v)
    {
        const auto clip = view_projection * Vec4(world, 1.0F);
        if (clip.w <= 0.0F)
            return false;

        const auto ndc = Vec3(clip) / clip.w;
        out_u = (ndc.x * 0.5F) + 0.5F;
        out_v = 0.5F - (ndc.y * 0.5F);
        return true;
    }

    bool CameraView::project_to_screen(const Vec3& world, float& out_u, float& out_v) const
    {
        return tbx::project_to_screen(
            camera.get_view_projection_matrix(position, rotation),
            world,
            out_u,
            out_v);
    }

    Ray CameraView::cursor_ray(float u, float v) const
    {
        const auto vp = camera.get_view_projection_matrix(position, rotation);
        const auto inverse_vp = glm::inverse(vp);
        const auto ndc_x = (u * 2.0F) - 1.0F;
        const auto ndc_y = 1.0F - (v * 2.0F);
        const auto unproject = [&](float ndc_z)
        {
            const auto point = inverse_vp * Vec4(ndc_x, ndc_y, ndc_z, 1.0F);
            return Vec3(point) / point.w;
        };
        const auto near_point = unproject(-1.0F);
        return Ray {
            .origin = near_point,
            .direction = glm::normalize(unproject(1.0F) - near_point),
        };
    }

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
        view.tags = camera_entity.get_tags();
        view.is_valid = true;
        return view;
    }
}
