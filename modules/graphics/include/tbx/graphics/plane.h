#pragma once

#include "tbx/math/math.h"
#include "tbx/tbx_api.h"
#include <cmath>

namespace tbx
{
    struct TBX_API Plane
    {
        vec3 normal = vec3(0.0f);
        float distance = 0.0f;

        void normalize()
        {
            const float length = std::sqrt(math::dot(normal, normal));
            if (length == 0.0f)
            {
                return;
            }

            normal *= (1.0f / length);
            distance /= length;
        }
    };
}

