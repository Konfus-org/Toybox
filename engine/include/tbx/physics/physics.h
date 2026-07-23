#pragma once
#include "tbx/api.h"
#include "tbx/assets/assets.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/events/events.h"
#include <memory>
#include <optional>

// The concrete physics boundary (see cmake/tbx_backend.cmake): physics/jolt/ implements it
// and its library types never escape that folder. The state is runtime.physics; gravity is a
// plain field applied every update.
namespace tbx
{
    /// @brief
    /// Purpose: What a raycast hit: the toy, where, and how far along the ray.
    struct TBX_DLL_EXPORT RaycastHit
    {
        ToyId toy = NULL_TOY;
        Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
        float distance = 0.0f;
    };

    /// @brief
    /// Purpose: The physics module's state, held by value on the Runtime. The simulation
    /// itself lives behind the backend seam (physics/jolt/ defines Simulation; library types
    /// never escape that folder) and is built lazily on the first update.
    struct TBX_DLL_EXPORT PhysicsState
    {
        PhysicsState();
        ~PhysicsState();

        PhysicsState(const PhysicsState&) = delete;
        PhysicsState& operator=(const PhysicsState&) = delete;

        Vec3 gravity = Vec3(0.0f, -9.81f, 0.0f);

        struct Simulation; // defined by the physics backend's .cpp
        std::unique_ptr<Simulation> simulation;
    };

    /// @brief
    /// Purpose: Casts a ray against the running world (tbx::get_runtime().physics) — the script-facing
    /// raycast. Main-thread only.
    TBX_DLL_EXPORT std::optional<RaycastHit> raycast(
        const Vec3& origin,
        const Vec3& direction,
        float max_distance);

}
