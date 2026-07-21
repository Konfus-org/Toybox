#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

// The glm math backend: supplies the tbx math types and the tbx::math function surface the
// engine programs against. Another backend folder (engine/src/math/<name>/) provides the same
// names to swap the library out (-DTBX_MATH_BACKEND=<name>).
namespace tbx
{
    using Vec2 = glm::vec2;
    using Vec3 = glm::vec3;
    using Vec4 = glm::vec4;
    using Quat = glm::quat;
    using Mat4 = glm::mat4;
}

namespace tbx::math
{
    /// @brief
    /// Purpose: Matrix inverse.
    inline Mat4 inverse(const Mat4& matrix)
    {
        return glm::inverse(matrix);
    }

    /// @brief
    /// Purpose: Vector length.
    inline float length(const Vec3& vector)
    {
        return glm::length(vector);
    }

    /// @brief
    /// Purpose: A view matrix looking from eye at center.
    inline Mat4 look_at(const Vec3& eye, const Vec3& center, const Vec3& up)
    {
        return glm::lookAt(eye, center, up);
    }

    /// @brief
    /// Purpose: Component-wise maximum.
    inline Vec3 max(const Vec3& a, const Vec3& b)
    {
        return glm::max(a, b);
    }

    /// @brief
    /// Purpose: Component-wise minimum.
    inline Vec3 min(const Vec3& a, const Vec3& b)
    {
        return glm::min(a, b);
    }

    /// @brief
    /// Purpose: Unit-length copy of a vector.
    inline Vec3 normalize(const Vec3& vector)
    {
        return glm::normalize(vector);
    }

    /// @brief
    /// Purpose: An orthographic projection.
    inline Mat4 orthographic(
        const float left,
        const float right,
        const float bottom,
        const float top,
        const float near_plane,
        const float far_plane)
    {
        return glm::ortho(left, right, bottom, top, near_plane, far_plane);
    }

    /// @brief
    /// Purpose: A perspective projection (field of view in radians).
    inline Mat4 perspective(
        const float fov_radians,
        const float aspect,
        const float near_plane,
        const float far_plane)
    {
        return glm::perspective(fov_radians, aspect, near_plane, far_plane);
    }

    /// @brief
    /// Purpose: An orientation whose -Z faces the given direction.
    inline Quat quat_look_at(const Vec3& direction, const Vec3& up)
    {
        return glm::quatLookAt(direction, up);
    }

    /// @brief
    /// Purpose: Degrees to radians.
    inline float radians(const float degrees)
    {
        return glm::radians(degrees);
    }

    /// @brief
    /// Purpose: Appends a scale to a matrix.
    inline Mat4 scale(const Mat4& matrix, const Vec3& factors)
    {
        return glm::scale(matrix, factors);
    }

    /// @brief
    /// Purpose: A rotation matrix from a quaternion.
    inline Mat4 to_mat4(const Quat& rotation)
    {
        return glm::mat4_cast(rotation);
    }

    /// @brief
    /// Purpose: Appends a translation to a matrix.
    inline Mat4 translate(const Mat4& matrix, const Vec3& offset)
    {
        return glm::translate(matrix, offset);
    }
}
