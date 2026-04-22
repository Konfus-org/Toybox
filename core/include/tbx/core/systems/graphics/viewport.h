#pragma once
#include "tbx/core/systems/math/size.h"
#include "tbx/core/systems/math/vectors.h"
#include "tbx/core/tbx_api.h"

namespace tbx
{
    struct TBX_API Viewport
    {
        Vec2 position = Vec2(0.0f);
        Size dimensions = {};
    };
}
