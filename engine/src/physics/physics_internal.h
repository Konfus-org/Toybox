#pragma once
#include "tbx/physics/physics.h"

namespace tbx::internal
{
    /// @brief
    /// Purpose: Casts a ray against the given simulated world; empty when nothing is hit. The
    /// public tbx::raycast forwards here with tbx::internal::get_runtime().physics.
    std::optional<RaycastHit> raycast(
        PhysicsState& physics,
        const Vec3& origin,
        const Vec3& direction,
        float max_distance);

    /// @brief
    /// Purpose: Advances the simulation one fixed step: mirrors the runtime sandbox's collider
    /// toys into the physics world (Shape::MESH colliders take their triangles from the toy's
    /// Renderer block, so mesh-collider toys must wear one), steps, writes dynamic poses back
    /// to Transforms, emits each RigidBody's collided signal, and queues collision events.
    void update_physics(
        PhysicsState& physics,
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        float fixed_delta_time);
}
