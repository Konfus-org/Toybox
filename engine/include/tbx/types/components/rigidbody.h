#pragma once
#include "tbx/types/components/component.h"
#include "tbx/types/vectors.h"

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
    [[tbx::serializable]];
    struct TBX_API Rigidbody : Component
    {
        [[tbx::prop]]
        float mass = 1.0F;

        [[tbx::prop]]
        bool is_kinematic = false;

        [[tbx::prop]]
        bool is_gravity_enabled = true;

        [[tbx::prop]]
        PhysicsTransformSyncMode transform_sync_mode = PhysicsTransformSyncMode::SWEEP;

        [[tbx::prop]]
        Vec3 linear_velocity = Vec3(0.0F, 0.0F, 0.0F);

        [[tbx::prop]]
        Vec3 angular_velocity = Vec3(0.0F, 0.0F, 0.0F);

        [[tbx::prop]]
        float friction = 0.5F;

        [[tbx::prop]]
        float restitution = 0.0F;

        [[tbx::prop]]
        float linear_damping = 0.05F;

        [[tbx::prop]]
        float angular_damping = 0.05F;

        [[tbx::prop]]
        bool is_sleep_enabled = true;

        [[tbx::prop]]
        float sleep_velocity_threshold = 0.03F;

        [[tbx::prop]]
        float sleep_time_seconds = 0.5F;

        bool is_valid() const;
    };

}

#include "tbx/types/components/rigidbody.generated.h"
