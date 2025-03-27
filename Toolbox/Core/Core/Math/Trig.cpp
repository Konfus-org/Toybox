#include "Core/ToolboxPCH.h"
#include "Core/Math/Trig.h"
#include <glm/glm.hpp>
#include <glm/ext/scalar_constants.hpp>

namespace Tbx::Math
{
    float PI()
    {
        return glm::pi<float>();
    }

    float DegreesToRadians(float degrees)
    {
        return glm::radians(degrees);
    }

    float RadiansToDegrees(float radians)
    {
        return glm::degrees(radians);
    }

    float Cos(float x)
    {
        return glm::cos(x);
    }

    float Sin(float x)
    {
        return glm::sin(x);
    }

    float Tan(float x)
    {
        return glm::tan(x);
    }

    float ACos(float x)
    {
        return glm::acos(x);
    }

    float ASin(float x)
    {
        return glm::asin(x);
    }

    float ATan(float x)
    {
        return glm::atan(x);
    }
}
