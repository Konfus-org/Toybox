#pragma once

#include "Tbx/DllExport.h"
#include "Tbx/Math/Vectors.h"
#include <cmath>

namespace Tbx
{
    struct EXPORT Plane
    {
        Vector3 Normal;
        float Distance;

        void Normalize()
        {
            const float length = std::sqrt(Vector3::Dot(Normal, Normal));
            Normal = Normal * (1.0f / length);
            Distance /= length;
        }
    };
}

