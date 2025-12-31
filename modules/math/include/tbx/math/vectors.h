#pragma once
#include <glm/glm.hpp>
namespace tbx
{
    /// <summary>
    /// Represents a two-component floating-point vector compatible with GLM operations.
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    /// </summary>
    struct Vec2 : glm::vec2
    {
    };

    /// <summary>
    /// Represents a three-component floating-point vector compatible with GLM operations.
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    /// </summary>
    struct Vec3 : glm::vec3
    {
    };

    /// <summary>
    /// Represents a four-component floating-point vector compatible with GLM operations.
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    /// </summary>
    struct Vec4 : glm::vec4
    {
    };

    /// <summary>
    /// Represents a two-component signed integer vector compatible with GLM operations.
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    /// </summary>
    struct IVec2 : glm::ivec2
    {
    };

    /// <summary>
    /// Represents a three-component signed integer vector compatible with GLM operations.
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    /// </summary>
    struct IVec3 : glm::ivec3
    {
    };

    /// <summary>
    /// Represents a four-component signed integer vector compatible with GLM operations.
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    /// </summary>
    struct IVec4 : glm::ivec4
    {
    };

    /// <summary>
    /// Represents a two-component unsigned integer vector compatible with GLM operations.
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    /// </summary>
    struct UVec2 : glm::uvec2
    {
    };

    /// <summary>
    /// Represents a three-component unsigned integer vector compatible with GLM operations.
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    /// </summary>
    struct UVec3 : glm::uvec3
    {
    };

    /// <summary>
    /// Represents a four-component unsigned integer vector compatible with GLM operations.
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    /// </summary>
    struct UVec4 : glm::uvec4
    {
    };
}
