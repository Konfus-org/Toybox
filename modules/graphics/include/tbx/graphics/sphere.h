#pragma once
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    // A sphere represented by a center point and radius.
    struct TBX_API Sphere
    {
        Vec3 center = Vec3(0.0f);
        float radius = 0.0f;
    };

    /// <summary>Provides a unit sphere primitive.</summary>
    /// <remarks>Purpose: Supplies a shared unit sphere instance for common queries.
    /// Ownership: Stored as a shared constant owned by the module.
    /// Thread Safety: Safe to read concurrently.</remarks>
    inline const Sphere sphere = {Vec3(0.0f), 1.0f};
}
