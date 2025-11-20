#pragma once
#include "tbx/graphics/plane.h"
#include "tbx/graphics/sphere.h"
#include "tbx/math/math.h"
#include "tbx/tbx_api.h"
#include <array>

namespace tbx
{
    class TBX_API Frustum
    {
    public:
        Frustum() = default;
        Frustum(const mat4& view_projection)
        {
            extract_planes(view_projection);
        }

        bool intersects(const Sphere& sphere) const
        {
            for (const auto& plane : _planes)
            {
                const float distance = math::dot(plane.normal, sphere.center) + plane.distance;
                if (distance < -sphere.radius)
                {
                    return false;
                }
            }
            return true;
        }

    private:
        void extract_planes(const mat4& view_projection)
        {
            const mat4& m = view_projection;

            _planes =
            {
                Plane{ vec3(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0]), m[3][3] + m[3][0] }, // Left
                Plane{ vec3(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0]), m[3][3] - m[3][0] }, // Right
                Plane{ vec3(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1]), m[3][3] + m[3][1] }, // Bottom
                Plane{ vec3(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1]), m[3][3] - m[3][1] }, // Top
                Plane{ vec3(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2]), m[3][3] + m[3][2] }, // Near
                Plane{ vec3(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2]), m[3][3] - m[3][2] }  // Far
            };

            for (auto& plane : _planes)
            {
                plane.normalize();
            }
        }

        std::array<Plane, 6> _planes = {};
    };
}

