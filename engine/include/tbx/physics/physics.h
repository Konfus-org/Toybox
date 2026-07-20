#pragma once
#include "tbx/core/math.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/events/events.h"
#include <optional>

namespace tbx
{
    /// @brief
    /// Purpose: Makes a collider toy dynamic: it falls, collides, and writes its simulated
    /// pose back into Transform. Colliders without one are static scenery.
    struct RigidBody
    {
        float mass = 1.0f;
        bool is_kinematic = false;
    };

    /// @brief
    /// Purpose: Box collision shape centered on the toy's Transform.
    struct BoxCollider
    {
        Vec3 half_extents = Vec3(0.5f, 0.5f, 0.5f);
    };

    /// @brief
    /// Purpose: What a raycast hit: the toy, where, and how far along the ray.
    struct RaycastHit
    {
        ToyId toy = NULL_TOY;
        Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
        float distance = 0.0f;
    };
}

// The concrete physics boundary (see cmake/tbx_backend.cmake): physics/jolt/ implements it and
// its library types never escape that folder. State lives in the backend's cpp; initialization
// is lazy on first step.
namespace tbx::physics
{
    /// @brief
    /// Purpose: Casts a ray against the simulated world; empty when nothing is hit.
    std::optional<RaycastHit> raycast(
        const Vec3& origin,
        const Vec3& direction,
        float max_distance);

    /// @brief
    /// Purpose: Registers RigidBody/BoxCollider blocks; run() calls this at boot (tests call
    /// it directly) so kits can carry them.
    void register_physics_blocks();

    /// @brief
    /// Purpose: Tears the simulation down; the next step() starts fresh. run() calls this at
    /// shutdown, tests between scenarios.
    void reset();

    /// @brief
    /// Purpose: Advances the simulation one fixed step: mirrors collider toys into the physics
    /// world, steps, writes dynamic poses back to Transforms, and emits collision events
    /// (delivered at the next pump drain).
    void step(Sandbox& sandbox, Events& events, float fixed_delta_time);
}
