#pragma once
#include "TbxPCH.h"
#include "TbxAPI.h"

namespace Tbx
{
    struct TBX_API Vector3
    {
    public:
        Vector3() = default;
        explicit(false) Vector3(float x) : X(x), Y(x), Z(x) {}
        Vector3(float x, float y, float z) : X(x), Y(y), Z(z) {}

        float X;
        float Y;
        float Z;

        std::string ToString() const { return std::format("({}, {}, {})", X, Y, Z); }

        static Vector3 Identity() { return Vector3(1, 1, 1); }
        static Vector3 Zero() { return Vector3(0, 0, 0); }
        static Vector3 One() { return Vector3(1, 1, 1); }
        static Vector3 Forward() { return Vector3(0, 0, 1); }
        static Vector3 Backward() { return Vector3(0, 0, -1); }
        static Vector3 Up() { return Vector3(0, 1, 0); }
        static Vector3 Down() { return Vector3(0, -1, 0); }
        static Vector3 Right() { return Vector3(1, 0, 0); }
        static Vector3 Left() { return Vector3(-1, 0, 0); }

        static Vector3 Normalize(const Vector3& vector);
        static Vector3 Add(const Vector3& lhs, const Vector3& rhs);
        static Vector3 Subtract(const Vector3& lhs, const Vector3& rhs);
        static Vector3 Multiply(const Vector3& lhs, const Vector3& rhs);

        friend Vector3 operator + (const Vector3& lhs, const Vector3& rhs) { return Add(lhs, rhs); }
        friend Vector3 operator - (const Vector3& lhs, const Vector3& rhs) { return Subtract(lhs, rhs); }
        friend Vector3 operator * (const Vector3& lhs, const Vector3& rhs) { return Multiply(lhs, rhs); }
    };

    struct TBX_API Vector2
    {
    public:
        Vector2() = default;
        explicit(false) Vector2(float x) : X(x), Y(x) {}
        Vector2(float x, float y) : X(x), Y(y) {}
        explicit(false) Vector2(Vector3 vector) : X(vector.X), Y(vector.Y) {}

        float X;
        float Y;

        static Vector2 Zero() { return Vector2(0, 0); }
        static Vector2 Identity() { return Vector2(1, 1); }
    };

    struct TBX_API Vector2I
    {
    public:
        Vector2I() = default;
        explicit(false) Vector2I(int x) : X(x), Y(x) {}
        explicit(false) Vector2I(Vector3 vector) : X((int)vector.X), Y((int)vector.Y) {}
        Vector2I(int x, int y) : X(x), Y(y) {}

        int X;
        int Y;

        static Vector2I Zero() { return Vector2I(0, 0); }
        static Vector2I Identity() { return Vector2I(1, 1); }
    };
}
