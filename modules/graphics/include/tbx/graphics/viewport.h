#pragma once
#include "tbx/math/math.h"
#include "tbx/std/size.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    struct TBX_API Viewport
    {
        math::vec2 position = math::vec2(0.0f);
        Size dimensions = {};
    };
}
