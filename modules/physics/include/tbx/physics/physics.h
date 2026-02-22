#pragma once
#include "tbx/math/vectors.h"
#include "tbx/physics/collider.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    /// <summary>
    /// Purpose: Defines per-entity rigid body configuration consumed by runtime physics backends.
    /// </summary>
    /// <remarks>
    /// Ownership: Value type that owns all component data by copy.
    /// Thread Safety: Safe for concurrent reads; synchronize external mutation.
    /// </remarks>
    struct TBX_API Physics
    {
        float mass = 1.0F;
        bool is_kinematic = false;
        bool is_sensor = false;
        bool is_gravity_enabled = true;
        Collider collider = {};

        Vec3 linear_velocity = Vec3(0.0F, 0.0F, 0.0F);
        Vec3 angular_velocity = Vec3(0.0F, 0.0F, 0.0F);

        float friction = 0.5F;
        float restitution = 0.0F;
        float default_linear_damping = 0.05F;
        float default_angular_damping = 0.05F;

        bool is_sleep_enabled = true;
        float sleep_velocity_threshold = 0.03F;
        float sleep_time_seconds = 0.5F;

        /// <summary>
        /// Purpose: Validates that this per-entity component can be consumed by a physics backend.
        /// </summary>
        /// <remarks>
        /// Ownership: Reads stored values only and does not transfer ownership.
        /// Thread Safety: Safe for concurrent reads when no writer mutates concurrently.
        /// </remarks>
        bool get_is_valid() const;
    };
}
