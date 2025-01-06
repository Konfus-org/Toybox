#pragma once
#include <numbers>

#define BIT(x) (1 << (x))

namespace Tbx::Math
{
    constexpr float PI = std::numbers::pi_v<float>;

    static float Cos(float x)
    {
        return std::cosf(x);
    }

    static float Sin(float x)
    {
        return std::sinf(x);
    }

    static float Tan(float x)
    {
        return std::tanf(x);
    }
}
