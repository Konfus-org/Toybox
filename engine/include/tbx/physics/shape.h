#pragma once
#include "tbx/utils/api.h"
#include "tbx/utils/typedefs.h"

namespace tbx::physics
{
    /// @brief
    /// Purpose: The shared spatial-shape vocabulary — colliders and spatial audio sources both
    /// speak it (BOX uses half_extents, SPHERE radius, CAPSULE radius + height, MESH collides
    /// with the toy's gfx::Renderer geometry — the toy must wear a gfx::Renderer block).
    enum class Shape : uint8
    {
        BOX,
        SPHERE,
        CAPSULE,
        MESH
    };
}
