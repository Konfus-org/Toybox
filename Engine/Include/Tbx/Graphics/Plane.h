#pragma once

#include "Tbx/DllExport.h"
#include "Tbx/Math/Vectors.h"
#include <cmath>

namespace Tbx
{
    struct EXPORT Plane
    {
        Vector3 Normal = Vector3::Zero;
        float Distance = 0.0f;

        void Normalize()
        {
            const float length = std::sqrt(Vector3::Dot(Normal, Normal));
            Normal = Normal * (1.0f / length);
            Distance /= length;
        }
    };
}

