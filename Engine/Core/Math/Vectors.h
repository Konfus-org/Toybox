#pragma once
#include "TbxPCH.h"
#include "TbxAPI.h"

namespace Tbx
{
    struct TBX_API Vector2
    {
    public:
        Vector2() = default;
        Vector2(float x, float y) : X(x), Y(y) {}

        float X;
        float Y;
    };

    struct TBX_API Vector2I
    {
    public:
        Vector2I() = default;
        Vector2I(int x, int y) : X(x), Y(y) {}

        int X;
        int Y;
    };

    struct TBX_API Vector3
    {
    public:
        Vector3() = default;
        Vector3(float x, float y, float z) : X(x), Y(y), Z(z) {}

        float X;
        float Y;
        float Z;

        static Vector3 Zero() { return Vector3(0, 0, 0); }
        static Vector3 Identity() { return Vector3(1, 1, 1); }

        friend Vector3 operator + (const Vector3& lhs, const Vector3& rhs) { return Vector3(lhs.X + rhs.X, lhs.Y + rhs.Y, lhs.Z + rhs.Z); }
        friend Vector3 operator - (const Vector3& lhs, const Vector3& rhs) { return Vector3(lhs.X - rhs.X, lhs.Y - rhs.Y, lhs.Z - rhs.Z); }
        friend Vector3 operator * (const Vector3& lhs, const Vector3& rhs) { return Vector3(lhs.X * rhs.X, lhs.Y * rhs.Y, lhs.Z * rhs.Z); }
        friend Vector3 operator / (const Vector3& lhs, const Vector3& rhs) { return Vector3(lhs.X / rhs.X, lhs.Y / rhs.Y, lhs.Z / rhs.Z); }
    };
}
