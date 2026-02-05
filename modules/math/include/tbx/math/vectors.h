#pragma once
#include "tbx/tbx_api.h"
#include <glm/glm.hpp>
namespace tbx
{
    /// <summary>
    /// Represents a two-component floating-point vector compatible with GLM operations.
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    /// </summary>
    using Vec2 = glm::vec2;

    /// <summary>
    /// Represents a three-component floating-point vector compatible with GLM operations.
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    /// </summary>
    using Vec3 = glm::vec3;

    /// <summary>
    /// Represents a four-component floating-point vector compatible with GLM operations.
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    /// </summary>
    using Vec4 = glm::vec4;

    /// <summary>
    /// Represents a two-component signed integer vector compatible with GLM operations.
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    /// </summary>
    using IVec2 = glm::ivec2;

    /// <summary>
    /// Represents a three-component signed integer vector compatible with GLM operations.
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    /// </summary>
    using IVec3 = glm::ivec3;

    /// <summary>
    /// Represents a four-component signed integer vector compatible with GLM operations.
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    /// </summary>
    using IVec4 = glm::ivec4;

    /// <summary>
    /// Represents a two-component unsigned integer vector compatible with GLM operations.
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    /// </summary>
    using UVec2 = glm::uvec2;

    /// <summary>
    /// Represents a three-component unsigned integer vector compatible with GLM operations.
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    /// </summary>
    using UVec3 = glm::uvec3;

    /// <summary>
    /// Represents a four-component unsigned integer vector compatible with GLM operations.
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    /// </summary>
    using UVec4 = glm::uvec4;

    /// <summary>
    /// Normalizes a two-component vector to unit length.
    /// Ownership: returns a value copy; the caller owns the result.
    /// Thread Safety: stateless; safe to call concurrently.
    /// </summary>
    TBX_API Vec2 normalize(Vec2 v);

    /// <summary>
    /// Normalizes a three-component vector to unit length.
    /// Ownership: returns a value copy; the caller owns the result.
    /// Thread Safety: stateless; safe to call concurrently.
    /// </summary>
    TBX_API Vec3 normalize(Vec3 v);

    /// <summary>
    /// Normalizes a four-component vector to unit length.
    /// Ownership: returns a value copy; the caller owns the result.
    /// Thread Safety: stateless; safe to call concurrently.
    /// </summary>
    TBX_API Vec4 normalize(Vec4 v);
}
