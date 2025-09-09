#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Plane.h"
#include "Tbx/Graphics/Sphere.h"
#include "Tbx/Math/Mat4x4.h"
#include "Tbx/Math/Vectors.h"
#include <array>

namespace Tbx
{
    class EXPORT Frustum
    {
    public:
        Frustum() = default;
        explicit Frustum(const Mat4x4& viewProjection)
        {
            ExtractPlanes(viewProjection);
        }

        bool Intersects(const Sphere& sphere) const
        {
            for (const auto& plane : _planes)
            {
                float distance = Vector3::Dot(plane.Normal, sphere.Center) + plane.Distance;
                if (distance < -sphere.Radius) return false;
            }
            return true;
        }

    private:
        void ExtractPlanes(const Mat4x4& viewProj)
        {
            const auto& m = viewProj.Values;

            _planes =
            {
                Plane{ { m[3] + m[0],  m[7] + m[4],  m[11] + m[8]  },  m[15] + m[12] }, // Left
                Plane{ { m[3] - m[0],  m[7] - m[4],  m[11] - m[8]  },  m[15] - m[12] }, // Right
                Plane{ { m[3] + m[1],  m[7] + m[5],  m[11] + m[9]  },  m[15] + m[13] }, // Bottom
                Plane{ { m[3] - m[1],  m[7] - m[5],  m[11] - m[9]  },  m[15] - m[13] }, // Top
                Plane{ { m[3] + m[2],  m[7] + m[6],  m[11] + m[10] },  m[15] + m[14] }, // Near
                Plane{ { m[3] - m[2],  m[7] - m[6],  m[11] - m[10] },  m[15] - m[14] }  // Far
            };

            for (auto& plane : _planes)
            {
                plane.Normalize();
            }
        }

        std::array<Plane, 6> _planes = {};
    };
}

