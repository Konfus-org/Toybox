#pragma once
#ifndef GLM_ENABLE_EXPERIMENTAL
    #define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include "tbx/types/vectors.generated.h"

namespace tbx
{
    const glm::vec3 UP = glm::vec3(0.0F, 1.0F, 0.0F);
    const glm::vec3 RIGHT = glm::vec3(1.0F, 0.0F, 0.0F);

    /// @brief
    /// Purpose: Represents a two-component floating-point vector compatible with GLM operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    [[serializable]];
    [[name("Vec2")]];
    [[array(2U)]];
    using Vec2 = glm::vec2;

    /// @brief
    /// Purpose: Represents a three-component floating-point vector compatible with GLM operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    [[serializable]];
    [[name("Vec3")]];
    [[array(3U)]];
    using Vec3 = glm::vec3;

    /// @brief
    /// Purpose: Represents a four-component floating-point vector compatible with GLM operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    [[serializable]];
    [[name("Vec4")]];
    [[array(4U)]];
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

    /// @brief
    /// Purpose: Signed angle from `a` to `b` measured about `axis` (radians). Zero when either vector
    /// is degenerate.
    /// @details
    /// Ownership: Returns a value type.
    /// Thread Safety: Stateless; safe to call concurrently.
    TBX_API float signed_angle(const Vec3& a, const Vec3& b, const Vec3& axis);

    /// @brief
    /// Purpose: Shortest distance from the 2D point (px,py) to the 2D segment (ax,ay)-(bx,by).
    /// @details
    /// Ownership: Returns a value type.
    /// Thread Safety: Stateless; safe to call concurrently.
    TBX_API float distance_point_segment(
        float px, float py, float ax, float ay, float bx, float by);
}
