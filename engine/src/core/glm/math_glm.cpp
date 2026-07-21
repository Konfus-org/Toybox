#include "tbx/core/math.h"
#include <glm/gtc/matrix_transform.hpp>

namespace tbx::math
{
    //// MATH (glm backend) ////

    Quat angle_axis(const float radians, const Vec3& axis)
    {
        return glm::angleAxis(radians, axis);
    }

    Mat4 inverse(const Mat4& matrix)
    {
        return glm::inverse(matrix);
    }

    float length(const Vec3& vector)
    {
        return glm::length(vector);
    }

    Mat4 look_at(const Vec3& eye, const Vec3& center, const Vec3& up)
    {
        return glm::lookAt(eye, center, up);
    }

    Vec3 max(const Vec3& a, const Vec3& b)
    {
        return glm::max(a, b);
    }

    Vec3 min(const Vec3& a, const Vec3& b)
    {
        return glm::min(a, b);
    }

    Vec3 normalize(const Vec3& vector)
    {
        return glm::normalize(vector);
    }

    Mat4 orthographic(
        const float left,
        const float right,
        const float bottom,
        const float top,
        const float near_plane,
        const float far_plane)
    {
        return glm::ortho(left, right, bottom, top, near_plane, far_plane);
    }

    Mat4 perspective(
        const float fov_radians,
        const float aspect,
        const float near_plane,
        const float far_plane)
    {
        return glm::perspective(fov_radians, aspect, near_plane, far_plane);
    }

    Quat quat_look_at(const Vec3& direction, const Vec3& up)
    {
        return glm::quatLookAt(direction, up);
    }

    float radians(const float degrees)
    {
        return glm::radians(degrees);
    }

    Mat4 scale(const Mat4& matrix, const Vec3& factors)
    {
        return glm::scale(matrix, factors);
    }

    Mat4 to_mat4(const Quat& rotation)
    {
        return glm::mat4_cast(rotation);
    }

    Mat4 translate(const Mat4& matrix, const Vec3& offset)
    {
        return glm::translate(matrix, offset);
    }
}
