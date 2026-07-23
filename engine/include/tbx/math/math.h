#pragma once
#include "tbx/api.h"
// The one math seam: the selected backend (cmake tbx_backend(MATH ...)) supplies the types via
// its <tbx_math_backend.h> and implements the tbx::math functions declared here in its own
// .cpp — swapped at link time like every other backend. Nothing else names the library.
#include <tbx_math_backend.h>

namespace tbx
{
    /// @brief
    /// Purpose: The position, rotation, and scale pulled out of an affine transform matrix
    /// (skew and perspective are discarded). The inverse of composing T * R * S.
    struct DecomposedTransform
    {
        Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
        Quat rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
        Vec3 scale = Vec3(1.0f, 1.0f, 1.0f);
    };

    /// @brief
    /// Purpose: A rotation of the given angle (radians) around an axis.
    TBX_API Quat angle_axis(float radians, const Vec3& axis);

    /// @brief
    /// Purpose: Splits an affine transform matrix into position/rotation/scale (e.g. a world
    /// transform composed up the parent chain).
    TBX_API DecomposedTransform decompose(const Mat4& matrix);

    /// @brief
    /// Purpose: Vector cross product.
    TBX_API Vec3 cross(const Vec3& a, const Vec3& b);

    /// @brief
    /// Purpose: Radians to degrees.
    TBX_API float degrees(float radians);

    /// @brief
    /// Purpose: Distance between two points.
    TBX_API float distance(const Vec3& a, const Vec3& b);

    /// @brief
    /// Purpose: Vector dot product.
    TBX_API float dot(const Vec3& a, const Vec3& b);

    /// @brief
    /// Purpose: A rotation from euler angles (radians, applied yaw-pitch-roll).
    TBX_API Quat from_euler(const Vec3& radians_per_axis);

    /// @brief
    /// Purpose: Matrix inverse.
    TBX_API Mat4 inverse(const Mat4& matrix);

    /// @brief
    /// Purpose: Vector length.
    TBX_API float length(const Vec3& vector);

    /// @brief
    /// Purpose: Linear interpolation between two values by t (unclamped).
    TBX_API float lerp(float from, float to, float t);

    /// @brief
    /// Purpose: Linear interpolation between two points by t (unclamped).
    TBX_API Vec3 lerp(const Vec3& from, const Vec3& to, float t);

    /// @brief
    /// Purpose: A view matrix looking from eye at center.
    TBX_API Mat4 look_at(const Vec3& eye, const Vec3& center, const Vec3& up);

    /// @brief
    /// Purpose: Component-wise maximum.
    TBX_API Vec3 max(const Vec3& a, const Vec3& b);

    /// @brief
    /// Purpose: Component-wise minimum.
    TBX_API Vec3 min(const Vec3& a, const Vec3& b);

    /// @brief
    /// Purpose: A point moved toward a target by at most max_delta (never overshoots).
    TBX_API Vec3 move_toward(const Vec3& current, const Vec3& target, float max_delta);

    /// @brief
    /// Purpose: Combined rotation: first b, then a.
    TBX_API Quat multiply(const Quat& a, const Quat& b);

    /// @brief
    /// Purpose: Unit-length copy of a vector.
    TBX_API Vec3 normalize(const Vec3& vector);

    /// @brief
    /// Purpose: An orthographic projection.
    TBX_API Mat4 orthographic(
        float left,
        float right,
        float bottom,
        float top,
        float near_plane,
        float far_plane);

    /// @brief
    /// Purpose: A perspective projection (field of view in radians).
    TBX_API Mat4 perspective(float fov_radians, float aspect, float near_plane, float far_plane);

    /// @brief
    /// Purpose: An orientation whose -Z faces the given direction.
    TBX_API Quat quat_look_at(const Vec3& direction, const Vec3& up);

    /// @brief
    /// Purpose: Degrees to radians.
    TBX_API float radians(float degrees);

    /// @brief
    /// Purpose: A vector mirrored off a surface with the given normal.
    TBX_API Vec3 reflect(const Vec3& vector, const Vec3& normal);

    /// @brief
    /// Purpose: A vector rotated by a quaternion.
    TBX_API Vec3 rotate(const Quat& rotation, const Vec3& vector);

    /// @brief
    /// Purpose: Appends a scale to a matrix.
    TBX_API Mat4 scale(const Mat4& matrix, const Vec3& factors);

    /// @brief
    /// Purpose: Spherical interpolation between two rotations by t (clamped shortest path).
    TBX_API Quat slerp(const Quat& from, const Quat& to, float t);

    /// @brief
    /// Purpose: Euler angles (radians per axis) of a rotation.
    TBX_API Vec3 to_euler(const Quat& rotation);

    /// @brief
    /// Purpose: A rotation matrix from a quaternion.
    TBX_API Mat4 to_mat4(const Quat& rotation);

    /// @brief
    /// Purpose: Appends a translation to a matrix.
    TBX_API Mat4 translate(const Mat4& matrix, const Vec3& offset);
}
