#pragma once
#include "Tbx/Debug/IPrintable.h"
#include "Tbx/DllExport.h"

namespace Tbx
{
    // Forward declarations
    struct TBX_EXPORT Vector2;
    struct TBX_EXPORT Vector3;

    /// <summary>
    /// Represents a position, scale, or direction in 3d space. X, Y, Z are stored as euler angles.
    /// </summary>
    struct TBX_EXPORT Vector3 : public IPrintable
    {
        Vector3() = default;
        explicit(false) Vector3(float all) : X(all), Y(all), Z(all) {}
        explicit(false) Vector3(const Vector2& vector);
        Vector3(float x, float y, float z) : X(x), Y(y), Z(z) {}

        friend Vector3 operator + (const Vector3& lhs, const Vector3& rhs) { return Add(lhs, rhs); }
        friend Vector3 operator - (const Vector3& lhs, const Vector3& rhs) { return Subtract(lhs, rhs); }
        friend Vector3 operator * (const Vector3& lhs, const Vector3& rhs) { return Multiply(lhs, rhs); }
        friend Vector3 operator * (const Vector3& lhs, float scalar) { return Multiply(lhs, scalar); }

        Vector3& operator += (const Vector3& other);
        Vector3& operator -= (const Vector3& other);
        Vector3& operator *= (const Vector3& other);
        Vector3& operator *= (float other);

        std::string ToString() const override;
        bool IsNearlyZero(float tolerance = 1e-6f) const;
        Vector3 Normalize() const { return Normalize(*this); }
        Vector3 Add(const Vector3& rhs) const { return Add(*this, rhs); }
        Vector3 Subtract(const Vector3& rhs) const { return Subtract(*this, rhs); }
        Vector3 Multiply(const Vector3& rhs) const { return Multiply(*this, rhs); }
        Vector3 Multiply(float scalar) const { return Multiply(*this, scalar); }
        Vector3 Cross(const Vector3& rhs) const { return Cross(*this, rhs); }
        float Dot(const Vector3& rhs) const { return Dot(*this, rhs); }

        static Vector3 Normalize(const Vector3& vector);
        static Vector3 Add(const Vector3& lhs, const Vector3& rhs);
        static Vector3 Subtract(const Vector3& lhs, const Vector3& rhs);
        static Vector3 Multiply(const Vector3& lhs, const Vector3& rhs);
        static Vector3 Multiply(const Vector3& lhs, float scalar);
        static Vector3 Cross(const Vector3& lhs, const Vector3& rhs);
        static float Dot(const Vector3& lhs, const Vector3& rhs);

        static Vector3 One;
        static Vector3 Zero;
        static Vector3 Identity;
        static Vector3 Forward;
        static Vector3 Backward;
        static Vector3 Up;
        static Vector3 Down;
        static Vector3 Left;
        static Vector3 Right;

        float X = 0;
        float Y = 0;
        float Z = 0;
    };

    /// <summary>
    /// Represents a position, scale, or direction in 2d space. X, Y are stored as euler angles.
    /// </summary>
    struct TBX_EXPORT Vector2 : public IPrintable
    {
        Vector2() = default;
        explicit(false) Vector2(float all) : X(all), Y(all) {}
        Vector2(float x, float y) : X(x), Y(y) {}
        explicit(false) Vector2(const Vector3& vector) : X(vector.X), Y(vector.Y) {}

        friend Vector2 operator + (const Vector2& lhs, const Vector2& rhs) { return Add(lhs, rhs); }
        friend Vector2 operator - (const Vector2& lhs, const Vector2& rhs) { return Subtract(lhs, rhs); }
        friend Vector2 operator * (const Vector2& lhs, const Vector2& rhs) { return Multiply(lhs, rhs); }
        friend Vector2 operator * (const Vector2& lhs, float scalar) { return Multiply(lhs, scalar); }

        Vector2& operator += (const Vector2& other);
        Vector2& operator -= (const Vector2& other);
        Vector2& operator *= (const Vector2& other);
        Vector2& operator *= (float other);

        std::string ToString() const override;
        bool IsNearlyZero(float tolerance = 1e-6f) const;
        Vector2 Normalize() const { return Normalize(*this); }
        Vector2 Add(const Vector2& rhs) const { return Add(*this, rhs); }
        Vector2 Subtract(const Vector2& rhs) const { return Subtract(*this, rhs); }
        Vector2 Multiply(const Vector2& rhs) const { return Multiply(*this, rhs); }
        Vector2 Multiply(float scalar) const { return Multiply(*this, scalar); }
        float Dot(const Vector2& rhs) const { return Dot(*this, rhs); }

        static Vector2 Normalize(const Vector2& vector);
        static Vector2 Add(const Vector2& lhs, const Vector2& rhs);
        static Vector2 Subtract(const Vector2& lhs, const Vector2& rhs);
        static Vector2 Multiply(const Vector2& lhs, const Vector2& rhs);
        static Vector2 Multiply(const Vector2& lhs, float scalar);
        static float Dot(const Vector2& lhs, const Vector2& rhs);

        static Vector2 One;
        static Vector2 Zero;
        static Vector2 Identity;
        static Vector2 Forward;
        static Vector2 Backward;
        static Vector2 Up;
        static Vector2 Down;
        static Vector2 Left;
        static Vector2 Right;

        float X = 0;
        float Y = 0;
    };
}
