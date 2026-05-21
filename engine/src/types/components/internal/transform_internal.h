#pragma once
#include "tbx/types/components/transform.h"
#include <cmath>

namespace tbx::internal
{
    static Vec3 divide_components(const Vec3& numerator, const Vec3& denominator)
    {
        static constexpr float component_epsilon = 0.000001F;
        auto result = Vec3(0.0F, 0.0F, 0.0F);
        result.x =
            std::abs(denominator.x) <= component_epsilon ? 0.0F : numerator.x / denominator.x;
        result.y =
            std::abs(denominator.y) <= component_epsilon ? 0.0F : numerator.y / denominator.y;
        result.z =
            std::abs(denominator.z) <= component_epsilon ? 0.0F : numerator.z / denominator.z;
        return result;
    }

}
