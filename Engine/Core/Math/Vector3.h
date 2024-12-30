#pragma once
#include "tbxAPI.h"

namespace Toybox
{
    struct TBX_API Vector3
    {
    public:
        Vector3() = default;
        Vector3(float x, float y, float z) : X(x), Y(y), Z(z) {}

        float X;
        float Y;
        float Z;
    };
}
