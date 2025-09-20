#pragma once
#include "Tbx/Core/StringConvertible.h"
#include "Tbx/DllExport.h"

namespace Tbx
{
    // Forward declarations
    struct Vector2;
    struct Vector2I;
    struct Vector3;

    /// <summary>
    /// Represents a position, scale, or direction in 3d space. X, Y, Z are stored as euler angles.
    /// </summary>
    struct EXPORT Vector3 : public IStringConvertible
    {
        Vector3() = default;
        explicit(false) Vector3(float all) : X(all), Y(all), Z(all) {}
        explicit(false) Vector3(const Vector2& vector);
        explicit(false) Vector3(const Vector2I& vector);
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

        /// <summary>
        /// Returns true if the vector is nearly zero in all components
        /// </summary>
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
    struct EXPORT Vector2 : public IStringConvertible
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

        /// <summary>
        /// Returns true if the vector is nearly zero in all components
        /// </summary>
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

    /// <summary>
    /// Represents a position, scale, or direction in 2d space. X, Y are stored as euler angles.
    /// </summary>
    struct EXPORT Vector2I : public IStringConvertible
    {
        Vector2I() = default;
        explicit(false) Vector2I(int all) : X(all), Y(all) {}
        explicit(false) Vector2I(const Vector2& vector) : X(static_cast<int>(vector.X)), Y(static_cast<int>(vector.Y)) {}
        explicit(false) Vector2I(const Vector3& vector) : X(static_cast<int>(vector.X)), Y(static_cast<int>(vector.Y)) {}
        Vector2I(int x, int y) : X(x), Y(y) {}

        friend Vector2I operator + (const Vector2I& lhs, const Vector2I& rhs) { return Add(lhs, rhs); }
        friend Vector2I operator - (const Vector2I& lhs, const Vector2I& rhs) { return Subtract(lhs, rhs); }
        friend Vector2I operator * (const Vector2I& lhs, const Vector2I& rhs) { return Multiply(lhs, rhs); }
        friend Vector2I operator * (const Vector2I& lhs, int scalar) { return Multiply(lhs, scalar); }

        Vector2I& operator += (const Vector2I& other);
        Vector2I& operator -= (const Vector2I& other);
        Vector2I& operator *= (const Vector2I& other);
        Vector2I& operator *= (int other);

        std::string ToString() const override;

        /// <summary>
        /// Returns true if the vector is nearly zero in all components
        /// </summary>
        bool IsNearlyZero(int tolerance = 0) const;

        Vector2I Normalize() const { return Normalize(*this); }
        Vector2I Add(const Vector2I& rhs) const { return Add(*this, rhs); }
        Vector2I Subtract(const Vector2I& rhs) const { return Subtract(*this, rhs); }
        Vector2I Multiply(const Vector2I& rhs) const { return Multiply(*this, rhs); }
        Vector2I Multiply(int scalar) const { return Multiply(*this, scalar); }
        int Dot(const Vector2I& rhs) const { return Dot(*this, rhs); }

        static Vector2I Normalize(const Vector2I& vector);
        static Vector2I Add(const Vector2I& lhs, const Vector2I& rhs);
        static Vector2I Subtract(const Vector2I& lhs, const Vector2I& rhs);
        static Vector2I Multiply(const Vector2I& lhs, const Vector2I& rhs);
        static Vector2I Multiply(const Vector2I& lhs, int scalar);
        static int Dot(const Vector2I& lhs, const Vector2I& rhs);

        static Vector2I One;
        static Vector2I Zero;
        static Vector2I Identity;
        static Vector2I Forward;
        static Vector2I Backward;
        static Vector2I Up;
        static Vector2I Down;
        static Vector2I Left;
        static Vector2I Right;

        int X = 0;
        int Y = 0;
    };
}
