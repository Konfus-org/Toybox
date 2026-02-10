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
