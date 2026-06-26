#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/matrices.h"
#include "tbx/types/vectors.h"

namespace tbx
{
    /// @brief
    /// Purpose: A ray for intersection queries.
    /// @details
    /// The direction need NOT be unit length. Reported hit distances are in units of `direction`, so a
    /// ray whose direction is a world-space ray transformed into some local space yields world-space
    /// distances — the convention viewport picking relies on (test in local space, compare in world units).
    /// Ownership: Value type. Thread Safety: Safe to copy between threads.
    struct TBX_API Ray
    {
        Vec3 origin = Vec3(0.0F);
        Vec3 direction = Vec3(0.0F, 0.0F, -1.0F);
    };

    /// @brief
    /// Purpose: Transforms a ray by an affine matrix — the origin as a point, the direction as a vector
    /// (so the direction picks up the matrix's scale; see the distance note on @ref Ray).
    TBX_API Ray transform_ray(const Mat4& matrix, const Ray& ray);

    /// @brief
    /// Purpose: Ray vs axis-aligned box (slab test). Reports the nearest entry distance in `out_distance`
    /// and counts only boxes entered IN FRONT of the origin (out_distance > 0); a box that encloses the
    /// origin is rejected. Returns whether the ray hits.
    TBX_API bool ray_intersects_aabb(
        const Ray& ray, const Vec3& minimum, const Vec3& maximum, float& out_distance);

    /// @brief
    /// Purpose: Möller–Trumbore ray vs triangle (double-sided). `out_distance` is along `ray.direction`
    /// and is only reported for hits in front of the origin. Returns whether the ray hits.
    TBX_API bool ray_intersects_triangle(
        const Ray& ray, const Vec3& a, const Vec3& b, const Vec3& c, float& out_distance);

    /// @brief
    /// Purpose: Nearest triangle of a mesh the ray actually hits (geometry-precise picking). Pass a ray
    /// already transformed into the mesh's local space; the mesh AABB is the broad-phase reject (which
    /// also excludes a box enclosing the origin). `out_distance` is in the ray-direction's units. Returns
    /// whether the ray hits any triangle.
    TBX_API bool ray_intersects_mesh(const Ray& local_ray, const Mesh& mesh, float& out_distance);

    /// @brief
    /// Purpose: Parameter along the axis line (through `axis_origin`, along unit `axis_direction`) of the
    /// point closest to `ray`. The parameter grows in the +axis_direction so a dragged handle follows the
    /// cursor. Returns 0 when the ray is (almost) parallel to the axis.
    TBX_API float closest_point_on_axis(
        const Ray& ray, const Vec3& axis_origin, const Vec3& axis_direction);

    /// @brief
    /// Purpose: Intersects `ray` with the plane through `plane_point` with normal `plane_normal`, writing
    /// the hit to `out_hit`. Returns false when (almost) parallel, or when the hit lies behind the ray
    /// origin (forward-only — a plane behind the origin would otherwise yield a hit behind the cursor).
    TBX_API bool ray_intersects_plane(
        const Ray& ray, const Vec3& plane_point, const Vec3& plane_normal, Vec3& out_hit);
}
