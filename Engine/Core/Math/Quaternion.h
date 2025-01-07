#pragma once
#include "TbxAPI.h"
#include "Vectors.h"
#include "Trig.h"

namespace Tbx
{
    struct TBX_API Quaternion
    {
    public:
        Quaternion() = default;
        explicit(false) Quaternion(Vector3 euler) { auto q = FromEuler(euler); X = q.X; Y = q.Y; Z = q.Z; W = q.W; }
        Quaternion(float x, float y, float z, float w) : X(x), Y(y), Z(z), W(w) {}

        float X;
        float Y;
        float Z;
        float W;

        static Quaternion Identity() { return Quaternion(0.0f, 0.0f, 0.0f, 1.0f); }
        static Quaternion FromEuler(const Vector3& euler) { return FromEuler(euler.X, euler.Y, euler.Z); }
        static Quaternion FromEuler(float x, float y, float z);
        static Vector3 ToEuler(const Quaternion& quaternion);

        static Quaternion Add(const Quaternion& lhs, const Quaternion& rhs);
        static Quaternion Subtract(const Quaternion& lhs, const Quaternion& rhs);
        static Quaternion Multiply(const Quaternion& lhs, const Quaternion& rhs);

        friend Quaternion operator+(const Quaternion& lhs, const Quaternion& rhs) { return Add(lhs, rhs); }
        friend Quaternion operator-(const Quaternion& lhs, const Quaternion& rhs) { return Subtract(lhs, rhs); }
        friend Quaternion operator*(const Quaternion& lhs, const Quaternion& rhs) { return Multiply(lhs, rhs); }
    };
}