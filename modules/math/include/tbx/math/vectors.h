#pragma once
#include "tbx/tbx_api.h"
#include <glm/glm.hpp>
namespace tbx
{
    /// @brief
    /// Purpose: Represents a two-component floating-point vector compatible with GLM operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using Vec2 = glm::vec2;

    /// @brief
    /// Purpose: Represents a three-component floating-point vector compatible with GLM operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using Vec3 = glm::vec3;

    /// @brief
    /// Purpose: Represents a four-component floating-point vector compatible with GLM operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using Vec4 = glm::vec4;

    /// @brief
    /// Purpose: Represents a two-component signed integer vector compatible with GLM operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using IVec2 = glm::ivec2;

    /// @brief
    /// Purpose: Represents a three-component signed integer vector compatible with GLM operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using IVec3 = glm::ivec3;

    /// @brief
    /// Purpose: Represents a four-component signed integer vector compatible with GLM operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using IVec4 = glm::ivec4;

    /// @brief
    /// Purpose: Represents a two-component unsigned integer vector compatible with GLM operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using UVec2 = glm::uvec2;

    /// @brief
    /// Purpose: Represents a three-component unsigned integer vector compatible with GLM
    /// operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using UVec3 = glm::uvec3;

    /// @brief
    /// Purpose: Represents a four-component unsigned integer vector compatible with GLM operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using UVec4 = glm::uvec4;

    /// @brief
    /// Purpose: Normalizes a two-component vector to unit length.
    /// @details
    /// Ownership: returns a value copy; the caller owns the result.
    /// Thread Safety: stateless; safe to call concurrently.
    TBX_API Vec2 normalize(Vec2 v);

    /// @brief
    /// Purpose: Normalizes a three-component vector to unit length.
    /// @details
    /// Ownership: returns a value copy; the caller owns the result.
    /// Thread Safety: stateless; safe to call concurrently.
    TBX_API Vec3 normalize(Vec3 v);

    /// @brief
    /// Purpose: Computes the dot product between two three-component vectors.
    /// @details
    /// Ownership: Returns a value copy; the caller owns the result.
    /// Thread Safety: Stateless; safe to call concurrently.
    TBX_API float dot(const Vec3& left, const Vec3& right);

    /// @brief
    /// Purpose: Computes the cross product between two three-component vectors.
    /// @details
    /// Ownership: Returns a value copy; the caller owns the result.
    /// Thread Safety: Stateless; safe to call concurrently.
    TBX_API Vec3 cross(const Vec3& left, const Vec3& right);

    /// @brief
    /// Purpose: Normalizes a four-component vector to unit length.
    /// @details
    /// Ownership: returns a value copy; the caller owns the result.
    /// Thread Safety: stateless; safe to call concurrently.
    TBX_API Vec4 normalize(Vec4 v);

    /// @brief
    /// Purpose: Computes the Euclidean distance between two Vec3 points.
    /// @details
    /// Ownership: Returns a value type.
    /// Thread Safety: Stateless; safe to call concurrently.
    TBX_API float distance(const Vec3& a, const Vec3& b);
}
