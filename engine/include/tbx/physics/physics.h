#pragma once
#include "tbx/core/api.h"
#include "tbx/core/math.h"
#include "tbx/ecs/builtin_blocks.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/events/events.h"
#include <optional>

namespace tbx
{
    /// @brief
    /// Purpose: What a raycast hit: the toy, where, and how far along the ray.
    struct TBX_API RaycastHit
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
    TBX_API std::optional<RaycastHit> raycast(
        const Vec3& origin,
        const Vec3& direction,
        float max_distance);

    /// @brief
    /// Purpose: Tears the simulation down; the next step() starts fresh. run() calls this at
    /// shutdown, tests between scenarios.
    TBX_API void reset();

    /// @brief
    /// Purpose: Advances the simulation one fixed step: mirrors collider toys into the physics
    /// world, steps, writes dynamic poses back to Transforms, and emits collision events
    /// (delivered at the next pump drain).
    TBX_API void update(Sandbox& sandbox, Events& events, float fixed_delta_time);
}
