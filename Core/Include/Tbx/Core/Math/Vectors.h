#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Debug/ILoggable.h"
#include <format>

namespace Tbx
{
    /// <summary>
    /// Represents a position, scale, or direction in 3d space. X, Y, Z are stored as euler angles.
    /// </summary>
    struct EXPORT Vector3 : public ILoggable
    {
    public:
        Vector3() = default;
        explicit(false) Vector3(float all) : X(all), Y(all), Z(all) {}
        Vector3(float x, float y, float z) : X(x), Y(y), Z(z) {}

        friend Vector3 operator + (const Vector3& lhs, const Vector3& rhs) { return Add(lhs, rhs); }
        friend Vector3 operator - (const Vector3& lhs, const Vector3& rhs) { return Subtract(lhs, rhs); }
        friend Vector3 operator * (const Vector3& lhs, const Vector3& rhs) { return Multiply(lhs, rhs); }
        friend Vector3 operator * (const Vector3& lhs, float scalar) { return Multiply(lhs, scalar); }

        std::string ToString() const override { return std::format("({}, {}, {})", X, Y, Z); }

        static Vector3 Identity() { return {1, 1, 1}; }
        static Vector3 Zero() { return {0, 0, 0}; }
        static Vector3 One() { return {1, 1, 1}; }
        /// <summary>
        /// This engine uses the Left-Handed coordinate system, so [0, 0, 1] is our forward vector.
        /// </summary>
        static Vector3 Forward() { return {0, 0, 1}; }
        /// <summary>
        /// This engine uses the Left-Handed coordinate system, so [0, 0, -1] is our backward vector.
        /// </summary>
        static Vector3 Backward() { return {0, 0, -1}; }
        static Vector3 Up() { return {0, 1, 0}; }
        static Vector3 Down() { return {0, -1, 0}; }
        static Vector3 Right() { return {1, 0, 0}; }
        static Vector3 Left() { return {-1, 0, 0}; }

        static Vector3 Normalize(const Vector3& vector);
        static Vector3 Add(const Vector3& lhs, const Vector3& rhs);
        static Vector3 Subtract(const Vector3& lhs, const Vector3& rhs);
        static Vector3 Multiply(const Vector3& lhs, const Vector3& rhs);
        static Vector3 Multiply(const Vector3& lhs, float scalar);
        static Vector3 Cross(const Vector3& lhs, const Vector3& rhs);
        static float Dot(const Vector3& lhs, const Vector3& rhs);

        float X = 0;
        float Y = 0;
        float Z = 0;
    };

    /// <summary>
    /// Represents a position, scale, or direction in 2d space. X, Y are stored as euler angles.
    /// </summary>
    struct EXPORT Vector2 : public ILoggable
    {
    public:
        Vector2() = default;
        explicit(false) Vector2(float x) : X(x), Y(x) {}
        Vector2(float x, float y) : X(x), Y(y) {}
        explicit(false) Vector2(Vector3 vector) : X(vector.X), Y(vector.Y) {}

        std::string ToString() const override { return std::format("({}, {})", X, Y); }

        static Vector2 Zero() { return {0, 0}; }
        static Vector2 Identity() { return {1, 1}; }

        float X = 0;
        float Y = 0;
    };

    /// <summary>
    /// Represents a position, scale, or direction in 2d space. X, Y are stored as euler angles.
    /// </summary>
    struct EXPORT Vector2I : public ILoggable
    {
    public:
        Vector2I() = default;
        explicit(false) Vector2I(int x) : X(x), Y(x) {}
        explicit(false) Vector2I(const Vector3& vector) : X(static_cast<int>(vector.X)), Y(static_cast<int>(vector.Y)) {}
        Vector2I(int x, int y) : X(x), Y(y) {}

        std::string ToString() const override { return std::format("({}, {})", X, Y); }

        static Vector2I Zero() { return {0, 0}; }
        static Vector2I Identity() { return {1, 1}; }

        int X = 0;
        int Y = 0;
    };
}
