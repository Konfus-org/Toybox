#pragma once
#include "tbxAPI.h"

namespace Toybox
{
    struct TBX_API Scale
    {
    public:
        Scale() = default;
        Scale(float x, float y, float z) : X(x), Y(y), Z(z) {}

        float X;
        float Y;
        float Z;
    };
}
