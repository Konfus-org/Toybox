#include "tbx/math/trig.h"
#include <glm/glm.hpp>

namespace tbx
{
    float to_radians(float degrees)
    {
        return glm::radians(degrees);
    }

    float to_degrees(float radians)
    {
        return glm::degrees(radians);
    }

    Vec3 to_radians(const Vec3& degrees)
    {
        return glm::radians(degrees);
    }

    Vec3 to_degrees(const Vec3& radians)
    {
        return glm::degrees(radians);
    }

    float min(float left, float right)
    {
        return glm::min(left, right);
    }

    float max(float left, float right)
    {
        return glm::max(left, right);
    }

    float clamp(float value, float minimum_value, float maximum_value)
    {
        return glm::clamp(value, minimum_value, maximum_value);
    }

    float sqrt(float value)
    {
        return glm::sqrt(value);
    }

    float floor(float value)
    {
        return glm::floor(value);
    }

    float ceil(float value)
    {
        return glm::ceil(value);
    }

    float cos(float x)
    {
        return glm::cos(x);
    }

    float sin(float x)
    {
        return glm::sin(x);
    }

    float tan(float x)
    {
        return glm::tan(x);
    }

    float acos(float x)
    {
        return glm::acos(x);
    }

    float asin(float x)
    {
        return glm::asin(x);
    }

    float atan(float x)
    {
        return glm::atan(x);
    }
}
