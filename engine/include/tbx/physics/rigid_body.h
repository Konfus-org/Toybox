#pragma once
#include "tbx/api.h"
#include "tbx/ecs/block.h"

namespace tbx::physics
{
    /// @brief
    /// Purpose: Makes a collider toy dynamic: it falls, collides, and writes its simulated
    /// pose back into Transform. Colliders without one are static scenery.
    struct TBX_API RigidBody : Block
    {
        float mass = 1.0f;
        bool is_kinematic = false;

        // Fluent setters — each returns *this for one-chain construction.
        RigidBody& set_mass(float value) { mass = value; return *this; }
        RigidBody& set_kinematic(bool value) { is_kinematic = value; return *this; }
    };
}
