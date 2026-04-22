#pragma once
#include "tbx/core/systems/math/vectors.h"
#include "tbx/core/tbx_api.h"

namespace tbx
{
    // A sphere represented by a center point and radius.
    struct TBX_API Sphere
    {
        Vec3 center = Vec3(0.0f);
        float radius = 0.0f;
    };

    /// @brief
    /// Purpose: Supplies a shared unit sphere instance for common queries.
    /// @details
    /// Ownership: Stored as a shared constant owned by the module.
    /// Thread Safety: Safe to read concurrently.
    // inline const Sphere sphere = {Vec3(0.0f), 1.0f};
}
