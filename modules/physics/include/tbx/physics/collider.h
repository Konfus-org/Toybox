#pragma once
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    /// <summary>
    /// Purpose: Configures mesh collider behavior for geometry sourced from a mesh component on the
    /// same entity.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns mesh collider settings by value only; geometry ownership stays with the mesh
    /// component.
    /// Thread Safety: Safe for concurrent reads; synchronize external mutation.
    /// </remarks>
    struct TBX_API MeshCollider
    {
        bool is_convex = true;
    };

    /// <summary>
    /// Purpose: Defines an axis-aligned box collider by half extents.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns size data by value.
    /// Thread Safety: Safe for concurrent reads; synchronize external mutation.
    /// </remarks>
    struct TBX_API CubeCollider
    {
        Vec3 half_extents = Vec3(0.5F, 0.5F, 0.5F);
    };

    /// <summary>
    /// Purpose: Defines a sphere collider by radius.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns radius data by value.
    /// Thread Safety: Safe for concurrent reads; synchronize external mutation.
    /// </remarks>
    struct TBX_API SphereCollider
    {
        float radius = 0.5F;
    };

    /// <summary>
    /// Purpose: Defines a capsule collider by radius and half-height.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns capsule dimensions by value.
    /// Thread Safety: Safe for concurrent reads; synchronize external mutation.
    /// </remarks>
    struct TBX_API CapsuleCollider
    {
        float radius = 0.5F;
        float half_height = 0.5F;
    };

}
