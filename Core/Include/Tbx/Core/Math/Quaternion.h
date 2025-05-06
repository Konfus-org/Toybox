#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Math/Vectors.h"
#include "Tbx/Core/Debug/ILoggable.h"
#include <format>

namespace Tbx
{
    /// <summary>
    /// Represents a rotation. X, Y, Z, W are stored as radians.
    /// </summary>
    struct EXPORT Quaternion : public ILoggable
    {
    public:
        Quaternion() = default;

        Quaternion(float x, float y, float z, float w)
            : X(x), Y(y), Z(z), W(w) {}

        explicit(false) Quaternion(const Vector3& euler) 
        { 
            const auto& q = FromEuler(euler); 
            X = q.X; Y = q.Y; Z = q.Z; W = q.W; 
        }

        friend Quaternion operator + (const Quaternion& lhs, const Quaternion& rhs) { return Add(lhs, rhs); }
        friend Quaternion operator - (const Quaternion& lhs, const Quaternion& rhs) { return Subtract(lhs, rhs); }
        friend Quaternion operator * (const Quaternion& lhs, const Quaternion& rhs) { return Multiply(lhs, rhs); }
        friend Vector3 operator * (const Quaternion& lhs, const Vector3& rhs) { return Multiply(lhs, rhs); }
        friend Vector3 operator * (const Vector3& lhs, const Quaternion& rhs) { return Multiply(lhs, rhs); }

        std::string ToString() const override { return std::format("(X: {}, Y: {}, Z: {}, W: {})", X, Y, Z, W); }

        static Quaternion Normalize(const Quaternion& quaternion);
        static Quaternion Add(const Quaternion& lhs, const Quaternion& rhs);
        static Quaternion Subtract(const Quaternion& lhs, const Quaternion& rhs);
        static Quaternion Multiply(const Quaternion& lhs, const Quaternion& rhs);
        static Vector3 Multiply(const Quaternion& lhs, const Vector3& rhs);
        static Vector3 Multiply(const Vector3& lhs, const Quaternion& rhs);

        static Quaternion Identity();
        static Quaternion FromAxisAngle(const Vector3& axis, float angle);
        static Quaternion FromEuler(const Vector3& euler) { return FromEuler(euler.X, euler.Y, euler.Z); }
        static Quaternion FromEuler(float x, float y, float z);
        static Vector3 ToEuler(const Quaternion& quaternion);

        float X = 0;
        float Y = 0;
        float Z = 0;
        float W = 1;
    };
}