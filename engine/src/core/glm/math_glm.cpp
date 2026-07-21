#include "tbx/core/math.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace tbx::math
{
    //// MATH (glm backend) ////

    Quat angle_axis(const float radians, const Vec3& axis)
    {
        return glm::angleAxis(radians, axis);
    }

    Vec3 cross(const Vec3& a, const Vec3& b)
    {
        return glm::cross(a, b);
    }

    float degrees(const float radians)
    {
        return glm::degrees(radians);
    }

    float distance(const Vec3& a, const Vec3& b)
    {
        return glm::distance(a, b);
    }

    float dot(const Vec3& a, const Vec3& b)
    {
        return glm::dot(a, b);
    }

    Quat from_euler(const Vec3& radians_per_axis)
    {
        return Quat(radians_per_axis);
    }

    Mat4 inverse(const Mat4& matrix)
    {
        return glm::inverse(matrix);
    }

    float length(const Vec3& vector)
    {
        return glm::length(vector);
    }

    float lerp(const float from, const float to, const float t)
    {
        return glm::mix(from, to, t);
    }

    Vec3 lerp(const Vec3& from, const Vec3& to, const float t)
    {
        return glm::mix(from, to, t);
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

    Vec3 move_toward(const Vec3& current, const Vec3& target, const float max_delta)
    {
        const Vec3 offset = target - current;
        const float remaining = glm::length(offset);
        if (remaining <= max_delta || remaining <= 0.0f)
            return target;
        return current + offset / remaining * max_delta;
    }

    Quat multiply(const Quat& a, const Quat& b)
    {
        return a * b;
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

    Vec3 reflect(const Vec3& vector, const Vec3& normal)
    {
        return glm::reflect(vector, normal);
    }

    Vec3 rotate(const Quat& rotation, const Vec3& vector)
    {
        return rotation * vector;
    }

    Mat4 scale(const Mat4& matrix, const Vec3& factors)
    {
        return glm::scale(matrix, factors);
    }

    Quat slerp(const Quat& from, const Quat& to, const float t)
    {
        return glm::slerp(from, to, t);
    }

    Vec3 to_euler(const Quat& rotation)
    {
        return glm::eulerAngles(rotation);
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
