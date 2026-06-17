#pragma once
#include "tbx/types/components/transform.h"
#include "tbx/types/sphere.h"
#include "tbx/types/vectors.h"

namespace tbx
{
    struct Mesh;

    /// @brief
    /// Purpose: Stores a local-space axis-aligned bound and its enclosing sphere.
    /// @details
    /// Ownership: Value type with copied bounds data.
    /// Thread Safety: Safe to copy between threads.
    struct TBX_API MeshBounds
    {
        Vec3 minimum = Vec3(-1.0F);
        Vec3 maximum = Vec3(1.0F);
        Sphere sphere = Sphere {.center = Vec3(0.0F), .radius = 1.0F};
        bool is_valid = true;
    };

    /// @brief
    /// Purpose: Computes local-space bounds from mesh vertex positions.
    /// @details
    /// Ownership: Writes the computed bounds into out_bounds.
    /// Thread Safety: Stateless helper; safe to call concurrently.
    TBX_API bool try_compute_mesh_bounds(const Mesh& mesh, MeshBounds& out_bounds);

    /// @brief
    /// Purpose: Recomputes and stores the mesh-local bounds on the mesh.
    /// @details
    /// Ownership: Mutates the provided mesh value.
    /// Thread Safety: Not thread-safe with concurrent mutation of the same mesh.
    TBX_API void update_mesh_bounds(Mesh& mesh);

    /// @brief
    /// Purpose: Transforms a local-space sphere by a world transform.
    /// @details
    /// Ownership: Returns a transformed sphere value owned by the caller.
    /// Thread Safety: Stateless helper; safe to call concurrently.
    TBX_API Sphere transform_sphere(const Sphere& local_sphere, const Transform& transform);
}
