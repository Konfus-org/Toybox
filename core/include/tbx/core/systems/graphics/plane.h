#pragma once
#include "tbx/core/systems/math/matrices.h"
#include "tbx/core/systems/math/vectors.h"
#include "tbx/core/tbx_api.h"
#include <cmath>
#include <glm/geometric.hpp>

namespace tbx
{
    struct TBX_API Plane
    {
        Vec3 normal = Vec3(0.0f);
        float distance = 0.0f;

        void normalize()
        {
            const float length_squared = glm::dot(normal, normal);
            if (length_squared == 0.0f)
            {
                return;
            }

            const float inverse_length = 1.0f / std::sqrt(length_squared);
            normal *= inverse_length;
            distance *= inverse_length;
        }
    };
}
