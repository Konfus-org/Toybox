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
}
