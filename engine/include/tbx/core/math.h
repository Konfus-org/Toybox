#pragma once
// The one math seam: the selected backend (cmake tbx_backend(MATH ...)) supplies the types via
// its <tbx_math_backend.h> and implements the tbx::math functions declared here in its own
// .cpp — swapped at link time like every other backend. Nothing else names the library.
#include <tbx_math_backend.h>


namespace tbx::math
{
    /// @brief
    /// Purpose: Matrix inverse.
    Mat4 inverse(const Mat4& matrix);

    /// @brief
    /// Purpose: Vector length.
    float length(const Vec3& vector);

    /// @brief
    /// Purpose: A view matrix looking from eye at center.
    Mat4 look_at(const Vec3& eye, const Vec3& center, const Vec3& up);

    /// @brief
    /// Purpose: Component-wise maximum.
    Vec3 max(const Vec3& a, const Vec3& b);

    /// @brief
    /// Purpose: Component-wise minimum.
    Vec3 min(const Vec3& a, const Vec3& b);

    /// @brief
    /// Purpose: Unit-length copy of a vector.
    Vec3 normalize(const Vec3& vector);

    /// @brief
    /// Purpose: An orthographic projection.
    Mat4 orthographic(
        float left,
        float right,
        float bottom,
        float top,
        float near_plane,
        float far_plane);

    /// @brief
    /// Purpose: A perspective projection (field of view in radians).
    Mat4 perspective(float fov_radians, float aspect, float near_plane, float far_plane);

    /// @brief
    /// Purpose: An orientation whose -Z faces the given direction.
    Quat quat_look_at(const Vec3& direction, const Vec3& up);

    /// @brief
    /// Purpose: Degrees to radians.
    float radians(float degrees);

    /// @brief
    /// Purpose: Appends a scale to a matrix.
    Mat4 scale(const Mat4& matrix, const Vec3& factors);

    /// @brief
    /// Purpose: A rotation matrix from a quaternion.
    Mat4 to_mat4(const Quat& rotation);

    /// @brief
    /// Purpose: Appends a translation to a matrix.
    Mat4 translate(const Mat4& matrix, const Vec3& offset);
}
