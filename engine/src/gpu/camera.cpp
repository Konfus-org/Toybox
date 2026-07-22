#include "tbx/gpu/camera.h"

namespace tbx::gpu
{
    Mat4 get_view_projection(
        const Camera& camera,
        const Mat4& world_matrix,
        const float aspect_ratio)
    {
        const Mat4 projection = perspective(
            radians(camera.fov_degrees),
            aspect_ratio,
            camera.near_plane,
            camera.far_plane);
        return projection * inverse(world_matrix);
    }

    Frustum make_frustum(
        const Camera& camera,
        const Mat4& world_matrix,
        const float aspect_ratio)
    {
        return tbx::make_frustum(get_view_projection(camera, world_matrix, aspect_ratio));
    }
}
