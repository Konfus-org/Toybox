#pragma once
#include "tbx/types/size.h"
#include "tbx/types/vectors.h"

namespace tbx
{
    struct TBX_API Viewport
    {
        Vec2 position = Vec2(0.0f);
        Size dimensions = {};

        bool is_zero()
        {
            return position.x == 0 && position.y == 0 && dimensions.width == 0
                   && dimensions.height == 0;
        }
    };
}
