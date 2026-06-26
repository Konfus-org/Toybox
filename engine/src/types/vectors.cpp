#include "tbx/types/vectors.h"
#include <algorithm>
#include <cmath>

namespace tbx
{
    Vec2 normalize(Vec2 v)
    {
        return glm::normalize(v);
    }

    Vec3 normalize(Vec3 v)
    {
        return glm::normalize(v);
    }

    float dot(const Vec3& left, const Vec3& right)
    {
        return glm::dot(left, right);
    }

    Vec3 cross(const Vec3& left, const Vec3& right)
    {
        return glm::cross(left, right);
    }

    Vec4 normalize(Vec4 v)
    {
        return glm::normalize(v);
    }

    float distance(const Vec3& a, const Vec3& b)
    {
        return glm::distance(a, b);
    }

    float signed_angle(const Vec3& a, const Vec3& b, const Vec3& axis)
    {
        if (glm::length(a) < 1e-6F || glm::length(b) < 1e-6F)
            return 0.0F;
        const auto x = glm::dot(a, b);
        const auto y = glm::dot(glm::cross(a, b), axis);
        return std::atan2(y, x);
    }

    float distance_point_segment(float px, float py, float ax, float ay, float bx, float by)
    {
        const auto dx = bx - ax;
        const auto dy = by - ay;
        const auto length_sq = (dx * dx) + (dy * dy);
        auto t = length_sq > 1e-12F ? (((px - ax) * dx) + ((py - ay) * dy)) / length_sq : 0.0F;
        t = std::clamp(t, 0.0F, 1.0F);
        const auto cx = ax + (t * dx);
        const auto cy = ay + (t * dy);
        return std::sqrt(((px - cx) * (px - cx)) + ((py - cy) * (py - cy)));
    }
}
