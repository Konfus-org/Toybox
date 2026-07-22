#pragma once
#include "tbx/ecs/block.h"
#include "tbx/utils/api.h"
#include "tbx/math/math.h"

namespace tbx
{
    /// @brief
    /// Purpose: The one engine-core spatial block: local position/rotation/scale. Hierarchy
    /// lives on the Sandbox (set_parent/get_parent), not inside the block.
    struct TBX_API Transform : Block
    {
        Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
        Quat rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
        Vec3 scale = Vec3(1.0f, 1.0f, 1.0f);
    };
}
