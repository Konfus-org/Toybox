#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Math/Vectors.h"
#include "Tbx/Math/Quaternion.h"
#include "Tbx/Math/Mat4x4.h"
#include "Tbx/Math/Bounds.h"

namespace Tbx::Constants
{
    EXPORT constexpr float PI = 3.14159265358979323846264338327950288f;

    namespace Vector3
    {
        EXPORT inline const Tbx::Vector3& One = { 1, 1, 1 };
        EXPORT inline const Tbx::Vector3& Zero = { 0, 0, 0 };
        EXPORT inline const Tbx::Vector3& Identity = One;
    }

    namespace Vector2
    {
        EXPORT inline const Tbx::Vector2& One = { 1, 1 };
        EXPORT inline const Tbx::Vector2& Zero = { 0, 0 };
        EXPORT inline const Tbx::Vector2& Identity = One;
    }

    namespace Vector2I
    {
        EXPORT inline const Tbx::Vector2I& One = { 1, 1 };
        EXPORT inline const Tbx::Vector2I& Zero = { 0, 0 };
        EXPORT inline const Tbx::Vector2I& Identity = One;
    }

    namespace Quaternion
    {
        EXPORT inline const Tbx::Quaternion& Identity = { 0, 0, 0, 1 };
    }

    namespace Mat4x4
    {
        EXPORT inline const Tbx::Mat4x4& Zero =
        {
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f
        };

        EXPORT inline const Tbx::Mat4x4& Identity =
        {
            { 1.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f, 1.0f }
        };
    }

    namespace Bounds
    {
        EXPORT inline const Tbx::Bounds& Identity = { -1.0f, 1.0f, -1.0f, 1.0f };
    }
}
