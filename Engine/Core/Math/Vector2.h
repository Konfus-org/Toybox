#pragma once
#include "tbxAPI.h"

namespace Tbx
{
    struct TBX_API Vector2
    {
    public:
        Vector2() = default;
        Vector2(float x, float y) : X(x), Y(y) {}

        float X;
        float Y;
    };


    struct TBX_API Vector2I
    {
    public:
        Vector2I() = default;
        Vector2I(int x, int y) : X(x), Y(y) {}

        int X;
        int Y;
    };
}
