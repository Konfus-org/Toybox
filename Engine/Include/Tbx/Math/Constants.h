#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Math/Vectors.h"
#include "Tbx/Math/Quaternion.h"
#include "Tbx/Math/Mat4x4.h"
#include "Tbx/Math/Bounds.h"

namespace Tbx::Consts
{
    EXPORT constexpr float PI = 3.14159265358979323846264338327950288f;

    namespace Vector3
    {
        EXPORT inline Tbx::Vector3 One = { 1, 1, 1 };
        EXPORT inline Tbx::Vector3 Zero = { 0, 0, 0 };
        EXPORT inline Tbx::Vector3 Identity = One;
        EXPORT inline Tbx::Vector3 Forward = { 0, 0, 1 };
        EXPORT inline Tbx::Vector3 Backward = { 0, 0, -1 };
        EXPORT inline Tbx::Vector3 Up = { 0, 1, 0 };
        EXPORT inline Tbx::Vector3 Down = { 0, -1, 0 };
        EXPORT inline Tbx::Vector3 Left = { 1, 0, 0 };
        EXPORT inline Tbx::Vector3 Right = { -1, 0, 0 };
    }

    namespace Vector2
    {
        EXPORT inline Tbx::Vector2 One = { 1, 1 };
        EXPORT inline Tbx::Vector2 Zero = { 0, 0 };
        EXPORT inline Tbx::Vector2 Identity = One;
        EXPORT inline Tbx::Vector2 Forward = { 0, 1 };
        EXPORT inline Tbx::Vector2 Backward = { 0, -1 };
        EXPORT inline Tbx::Vector2 Up = { 0, 1 };
        EXPORT inline Tbx::Vector2 Down = { 0, -1 };
        EXPORT inline Tbx::Vector2 Left = { 1, 0 };
        EXPORT inline Tbx::Vector2 Right = { -1, 0 };
    }

    namespace Vector2I
    {
        EXPORT inline Tbx::Vector2I One = { 1, 1 };
        EXPORT inline Tbx::Vector2I Zero = { 0, 0 };
        EXPORT inline Tbx::Vector2I Identity = One;
        EXPORT inline Tbx::Vector2I Forward = { 0, 1 };
        EXPORT inline Tbx::Vector2I Backward = { 0, -1 };
        EXPORT inline Tbx::Vector2I Up = { 0, 1 };
        EXPORT inline Tbx::Vector2I Down = { 0, -1 };
        EXPORT inline Tbx::Vector2I Left = { 1, 0 };
        EXPORT inline Tbx::Vector2I Right = { -1, 0 };
    }

    namespace Quaternion
    {
        EXPORT inline Tbx::Quaternion Identity = { 0, 0, 0, 1 };
    }

    namespace Mat4x4
    {
        EXPORT inline Tbx::Mat4x4 Zero =
        {
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f
        };

        EXPORT inline Tbx::Mat4x4 Identity =
        {
            { 1.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f, 1.0f }
        };
    }

    namespace Bounds
    {
        EXPORT inline Tbx::Bounds Identity = { -1.0f, 1.0f, -1.0f, 1.0f };
    }
}
