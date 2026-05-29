#pragma once
#include "tbx/types/components/component.h"
#include "tbx/types/components/rigidbody.generated.h"
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
    [[serializable]];
    struct TBX_API Rigidbody : Component
    {
        [[prop]]
        float mass = 1.0F;

        [[prop]]
        bool is_kinematic = false;

        [[prop]]
        bool is_gravity_enabled = true;

        [[prop]]
        PhysicsTransformSyncMode transform_sync_mode = PhysicsTransformSyncMode::SWEEP;

        [[prop]]
        Vec3 linear_velocity = Vec3(0.0F, 0.0F, 0.0F);

        [[prop]]
        Vec3 angular_velocity = Vec3(0.0F, 0.0F, 0.0F);

        [[prop]]
        float friction = 0.5F;

        [[prop]]
        float restitution = 0.0F;

        [[prop]]
        float linear_damping = 0.05F;

        [[prop]]
        float angular_damping = 0.05F;

        [[prop]]
        bool is_sleep_enabled = true;

        [[prop]]
        float sleep_velocity_threshold = 0.03F;

        [[prop]]
        float sleep_time_seconds = 0.5F;

        bool is_valid() const;
    };

}
