#pragma once
#include "Tbx/Core/StringConvertible.h"
#include "Tbx/DllExport.h"
#include "Tbx/Math/Vectors.h"

namespace Tbx
{
    /// <summary>
    /// Represents a rotation. X, Y, Z, W are stored as radians.
    /// </summary>
    struct EXPORT Quaternion : public IStringConvertible
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

        std::string ToString() const override;

        static Quaternion Normalize(const Quaternion& quaternion);
        static Quaternion Add(const Quaternion& lhs, const Quaternion& rhs);
        static Quaternion Subtract(const Quaternion& lhs, const Quaternion& rhs);
        static Quaternion Multiply(const Quaternion& lhs, const Quaternion& rhs);
        static Vector3 Multiply(const Quaternion& lhs, const Vector3& rhs);
        static Vector3 Multiply(const Vector3& lhs, const Quaternion& rhs);

        /// <summary>
        /// Gets the local right from a rotation.
        /// </summary>
        static Vector3 GetRight(const Quaternion& rot);
        /// <summary>
        /// Gets the local forward from a rotation.
        /// </summary>
        static Vector3 GetForward(const Quaternion& rot);
        /// <summary>
        /// Gets the local forward from a rotation.
        /// </summary>
        static Vector3 GetUp(const Quaternion& rot);

        static Quaternion FromAxisAngle(const Vector3& axis, float angle);
        static Quaternion FromEuler(const Vector3& euler) { return FromEuler(euler.X, euler.Y, euler.Z); }
        static Quaternion FromEuler(float x, float y, float z);
        static Vector3 ToEuler(const Quaternion& quaternion);

        static bool IsEqualOrEquivalent(const Quaternion& lhs, const Quaternion& rhs, float epsilon = 1e-5f);

        static Quaternion Identity;

        float X = 0;
        float Y = 0;
        float Z = 0;
        float W = 1;
    };
}