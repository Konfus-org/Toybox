#pragma once
#include <glm/gtc/quaternion.hpp>
namespace tbx
{
    // Represents a quaternion used for rotations compatible with GLM operations.
    // Ownership: value type; callers own any copies created from this alias.
    // Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    struct Quat : glm::quat
    {
    };
}
