#pragma once
#include "tbx/systems/math/size.h"
#include "tbx/systems/math/vectors.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    struct TBX_API Viewport
    {
        Vec2 position = Vec2(0.0f);
        Size dimensions = {};
    };
}
