#pragma once
#include "tbxapi.h"

namespace Toybox
{
    struct TBX_API Vector2
    {
    public:
        Vector2() = default;
        Vector2(float x, float y) : X(x), Y(y) {}

        float X;
        float Y;
    };
}
