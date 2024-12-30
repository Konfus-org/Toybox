#pragma once
#include "tbxAPI.h"

namespace Toybox
{
    struct TBX_API Quaternion
    {
        Quaternion(float x, float y, float z, float w) : X(x), Y(y), Z(z), W(w) {}

        float X;
        float Y;
        float Z;
        float W;
    };
}