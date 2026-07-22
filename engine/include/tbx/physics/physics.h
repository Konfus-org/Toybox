#pragma once
#include "tbx/api.h"
#include "tbx/assets/assets.h"
#include "tbx/ecs/registry.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/events/events.h"
#include "tbx/math/math.h"
#include <memory>
#include <optional>

// The concrete physics boundary (see cmake/tbx_backend.cmake): physics/jolt/ implements it
// and its library types never escape that folder. The state is runtime.physics; gravity is a
// plain field applied every update.
namespace tbx::physics
{
    /// @brief
    /// Purpose: What a raycast hit: the toy, where, and how far along the ray.
    struct TBX_API RaycastHit
    {
        ecs::ToyId toy = ecs::NULL_TOY;
        Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
        float distance = 0.0f;
    };

    /// @brief
    /// Purpose: The physics module's state, held by value on the Runtime. The simulation
    /// itself lives behind the backend seam (physics/jolt/ defines Simulation; library types
    /// never escape that folder) and is built lazily on the first update.
    struct TBX_API State
    {
        State();
        ~State();

        State(const State&) = delete;
        State& operator=(const State&) = delete;

        Vec3 gravity = Vec3(0.0f, -9.81f, 0.0f);
        struct Simulation; // defined by the physics backend's .cpp
        std::unique_ptr<Simulation> simulation;
    };

    /// @brief
    /// Purpose: Casts a ray against the simulated world; empty when nothing is hit.
    TBX_API std::optional<RaycastHit> raycast(
        State& physics,
        const Vec3& origin,
        const Vec3& direction,
        float max_distance);

    /// @brief
    /// Purpose: Advances the simulation one fixed step: mirrors the runtime sandbox's collider
    /// toys into the physics world (Shape::MESH colliders take their triangles from the toy's
    /// gpu::Renderer block, so mesh-collider toys must wear one), steps, writes dynamic poses back
    /// to Transforms, and emits collision events (delivered at the next pump drain).
    TBX_API void update(
        State& physics,
        ecs::Sandbox& sandbox,
        assets::State& assets,
        events::State& events,
        float fixed_delta_time);
}
