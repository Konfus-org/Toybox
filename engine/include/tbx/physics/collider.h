#pragma once
#include "tbx/ecs/block.h"
#include "tbx/math/math.h"
#include "tbx/physics/shape.h"
#include "tbx/utils/api.h"

namespace tbx
{
    /// @brief
    /// Purpose: Collision shape centered on the toy's Transform, described by Shape.
    struct TBX_API Collider : Block
    {
        Shape shape = Shape::BOX;
        Vec3 half_extents = Vec3(0.5f, 0.5f, 0.5f);
        float radius = 0.5f;
        float height = 1.0f;
    };
}
