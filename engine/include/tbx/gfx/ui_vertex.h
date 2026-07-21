#pragma once
#include "tbx/core/api.h"
#include "tbx/core/math.h"
#include "tbx/core/typedefs.h"

namespace tbx::gpu
{
    /// @brief
    /// Purpose: One 2D UI vertex: screen-space position, packed RGBA (premultiplied), uv.
    struct TBX_API UiVertex
    {
        Vec2 position = Vec2(0.0f, 0.0f);
        uint32 color = 0xFFFFFFFF;
        Vec2 uv = Vec2(0.0f, 0.0f);
    };
}
