#pragma once
// The one math seam: the selected backend (cmake tbx_backend(MATH ...)) provides the
// Vec2/Vec3/Vec4/Quat/Mat4 types and the tbx::math function surface. Nothing else names the
// library.
#include <tbx_math_backend.h>

namespace tbx
{
    /// @brief
    /// Purpose: Linear-space RGBA color, components in [0, 1] (engine-owned, backend-free).
    struct Color
    {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float a = 1.0f;
    };
}
