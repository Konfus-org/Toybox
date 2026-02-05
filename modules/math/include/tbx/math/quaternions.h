#pragma once
#include "tbx/tbx_api.h"
#include <glm/gtc/quaternion.hpp>
namespace tbx
{
    /// <summary>
    /// Represents a quaternion used for rotations compatible with GLM operations.
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    /// </summary>
    using Quat = glm::quat;

    /// <summary>
    /// Normalizes a quaternion to unit length.
    /// Ownership: returns a value copy; the caller owns the result.
    /// Thread Safety: stateless; safe to call concurrently.
    /// </summary>
    TBX_API Quat normalize(Quat q);
}
