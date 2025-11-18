#pragma once
#include "tbx/math/math.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    /// <summary>
    /// A sphere represented by a center point and radius.
    /// </summary>
    struct TBX_API Sphere
    {
        math::vec3 center = math::vec3(0.0f);
        float radius = 0.0f;
    };
}
