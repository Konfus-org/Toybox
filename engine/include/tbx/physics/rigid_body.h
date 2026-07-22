#pragma once
#include "tbx/ecs/block.h"
#include "tbx/utils/api.h"

namespace tbx
{
    /// @brief
    /// Purpose: Makes a collider toy dynamic: it falls, collides, and writes its simulated
    /// pose back into Transform. Colliders without one are static scenery.
    struct TBX_API RigidBody : ecs::Block
    {
        float mass = 1.0f;
        bool is_kinematic = false;
    };
}
