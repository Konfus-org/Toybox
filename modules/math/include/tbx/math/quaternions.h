#pragma once
#include "tbx/tbx_api.h"
#include <glm/gtc/quaternion.hpp>
namespace tbx
{
    /// @brief
    /// Purpose: Represents a quaternion used for rotations compatible with GLM operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.

    using Quat = glm::quat;

    /// @brief
    /// Purpose: Normalizes a quaternion to unit length.
    /// @details
    /// Ownership: returns a value copy; the caller owns the result.
    /// Thread Safety: stateless; safe to call concurrently.

    TBX_API Quat normalize(Quat q);
}
