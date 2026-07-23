#include "tbx/math/frustum.h"
#include "tbx/math/math.h"
#include "tbx/math/transform.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace tbx
{
    //// MATH (glm backend) ////

    Frustum make_frustum(const Mat4& view_projection)
    {
        // Gribb–Hartmann row extraction: each clip half-space is a row combination of the
        // combined matrix. glm matrices are column-major, so a "row" gathers one component
        // across the four columns.
        const auto row = [&view_projection](const int component)
        {
            return Vec4(
                view_projection[0][component],
                view_projection[1][component],
                view_projection[2][component],
                view_projection[3][component]);
        };
        const Vec4 row_x = row(0);
        const Vec4 row_y = row(1);
        const Vec4 row_z = row(2);
        const Vec4 row_w = row(3);

        auto frustum = Frustum {};
        frustum.planes[0] = row_w + row_x; // left
        frustum.planes[1] = row_w - row_x; // right
        frustum.planes[2] = row_w + row_y; // bottom
        frustum.planes[3] = row_w - row_y; // top
        frustum.planes[4] = row_w + row_z; // near
        frustum.planes[5] = row_w - row_z; // far
        for (Vec4& plane : frustum.planes)
        {
            // Unit normals are what make the sphere test's radius (and the streaming
            // margins added to it) metric distances.
            const float magnitude = glm::length(Vec3(plane.x, plane.y, plane.z));
            if (magnitude > 0.0f)
                plane /= magnitude;
        }
        return frustum;
    }

    bool intersects(const Frustum& frustum, const Vec3& sphere_center, const float sphere_radius)
    {
        for (const Vec4& plane : frustum.planes)
        {
            const float distance =
                glm::dot(Vec3(plane.x, plane.y, plane.z), sphere_center) + plane.w;
            if (distance < -sphere_radius)
                return false; // wholly behind one plane: out of sight
        }
        return true;
    }

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

    Transform decompose(const Mat4& matrix)
    {
        auto result = Transform();
        auto skew = Vec3(0.0f);
        auto perspective = Vec4(0.0f);
        glm::decompose(matrix, result.scale, result.rotation, result.position, skew, perspective);
        return result;
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
