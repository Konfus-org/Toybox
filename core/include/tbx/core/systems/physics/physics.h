#pragma once
#include "tbx/core/systems/math/vectors.h"
#include "tbx/core/tbx_api.h"

namespace tbx
{
    /// @brief
    /// Purpose: Selects how runtime physics applies script-authored Transform changes on
    /// non-kinematic rigid bodies.
    /// @details
    /// Ownership: Value enum copied by value.
    /// Thread Safety: Immutable enum values; safe for concurrent reads.
    enum class PhysicsTransformSyncMode
    {
        NONE = 0,
        TELEPORT = 1,
        SWEEP = 2,
    };

    /// @brief
    /// Purpose: Defines per-entity rigid body configuration consumed by runtime physics backends.
    /// @details
    /// Ownership: Value type that owns all component data by copy.
    /// Thread Safety: Safe for concurrent reads; synchronize external mutation.
    struct TBX_API Physics
    {
        float mass = 1.0F;
        bool is_kinematic = false;
        bool is_gravity_enabled = true;
        PhysicsTransformSyncMode transform_sync_mode = PhysicsTransformSyncMode::SWEEP;

        Vec3 linear_velocity = Vec3(0.0F, 0.0F, 0.0F);
        Vec3 angular_velocity = Vec3(0.0F, 0.0F, 0.0F);

        float friction = 0.5F;
        float restitution = 0.0F;
        float linear_damping = 0.05F;
        float angular_damping = 0.05F;

        bool is_sleep_enabled = true;
        float sleep_velocity_threshold = 0.03F;
        float sleep_time_seconds = 0.5F;

        bool is_valid() const;
    };
}
