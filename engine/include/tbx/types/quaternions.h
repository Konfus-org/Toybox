#pragma once
#include <glm/gtc/quaternion.hpp>
#include "tbx/types/quaternions.generated.h"
#include "tbx/types/vectors.h"

namespace tbx
{
    /// @brief
    /// Purpose: Represents a quaternion used for rotations compatible with GLM operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    [[serializable]];
    [[name("Quat")]];
    [[array(4U)]];
    using Quat = glm::quat;

    /// @brief
    /// Purpose: Normalizes a quaternion to unit length.
    /// @details
    /// Ownership: returns a value copy; the caller owns the result.
    /// Thread Safety: stateless; safe to call concurrently.
    TBX_API Quat normalize(Quat q);

    /// @brief
    /// Purpose: Builds a rotation whose forward axis (camera -Z) points along `forward`, keeping the
    /// horizon level against `world_up`; a near-vertical forward falls back to a different up axis so
    /// the horizon stays stable. Returns a normalized quaternion.
    /// @details
    /// Ownership: returns a value copy; the caller owns the result.
    /// Thread Safety: stateless; safe to call concurrently.
    TBX_API Quat look_rotation(const Vec3& forward, const Vec3& world_up);
}
