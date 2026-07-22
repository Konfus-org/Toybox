#pragma once
#include "tbx/api.h"
#include <tbx_math_backend.h>

namespace tbx
{
    /// @brief
    /// Purpose: A view volume as six inward-facing planes (xyz = unit normal, w = distance).
    /// Build one from the view-projection matrix a camera draws with (gpu::make_frustum) —
    /// streaming asks it which bounds spheres are in sight.
    struct TBX_API Frustum
    {
        Vec4 planes[6] = {};
    };

    /// @brief
    /// Purpose: The frustum of a view-projection matrix — the exact matrix the renderer
    /// draws with, so streaming matches rendering. Planes come out unit-length, which is
    /// what makes metric margins on the sphere test meaningful.
    TBX_API Frustum make_frustum(const Mat4& view_projection);

    /// @brief
    /// Purpose: Whether a bounds sphere touches the frustum (inflate the radius to add a
    /// streaming margin).
    TBX_API bool intersects(const Frustum& frustum, const Vec3& sphere_center, float sphere_radius);
}
