#pragma once
#include "tbx/api.h"
#include "tbx/ecs/block.h"
#include "tbx/math/math.h"
#include "tbx/physics/shape.h"


namespace tbx::physics
{
    /// @brief
    /// Purpose: Collision shape centered on the toy's Transform, described by Shape.
    struct TBX_API Collider : Block
    {
        Shape shape = Shape::BOX;
        Vec3 half_extents = Vec3(0.5f, 0.5f, 0.5f);
        float radius = 0.5f;
        float height = 1.0f;

        // Fluent setters — each returns *this for one-chain construction.
        Collider& set_shape(Shape value) { shape = value; return *this; }
        Collider& set_half_extents(Vec3 value) { half_extents = value; return *this; }
        Collider& set_radius(float value) { radius = value; return *this; }
        Collider& set_height(float value) { height = value; return *this; }
    };
}
