#pragma once
#include "tbx/api.h"
#include "tbx/ecs/block.h"
#include "tbx/ecs/toy.h"
#include "tbx/events/signal.h"
#include "tbx/reflection/attributes.h"

namespace tbx
{
    /// @brief
    /// Purpose: Makes a collider toy dynamic: it falls, collides, and writes its simulated
    /// pose back into Transform. Colliders without one are static scenery.
    struct TBX_SERIALIZABLE() TBX_EXPOSED_TO_SCRIPTING TBX_DLL_EXPORT RigidBody : Block
    {
        float mass = 1.0f;
        bool is_kinematic = false;

        // The event this body raises when it starts touching another — the physics system emits it on
        // the main thread with the other toy. Reflected automatically (Signal member), so scripts
        // connect with `toy.RigidBody.collided:connect(fn)`.
        Signal<Toy> collided;

        // Fluent setters — each returns *this for one-chain construction.
        RigidBody& set_mass(float value) { mass = value; return *this; }
        RigidBody& set_kinematic(bool value) { is_kinematic = value; return *this; }
    };
}
