#pragma once
#include "tbx/math/math.h"
#include "tbx/math/size.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    struct TBX_API Viewport
    {
        vec2 position = vec2(0.0f);
        Size dimensions = {};
    };
}
