#pragma once
#include "tbx/api.h"
#include "tbx/utils/typedefs.h"

namespace tbx::physics
{
    /// @brief
    /// Purpose: The shared spatial-shape vocabulary — colliders and spatial audio sources both
    /// speak it (BOX uses half_extents, SPHERE radius, CAPSULE radius + height, MESH collides
    /// with the toy's gpu::Renderer geometry — the toy must wear a gpu::Renderer block).
    enum class Shape : uint8
    {
        BOX,
        SPHERE,
        CAPSULE,
        MESH
    };
}
