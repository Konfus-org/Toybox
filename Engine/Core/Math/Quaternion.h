#pragma once
#include "TbxAPI.h"
#include "Vectors.h"
#include "Operations.h"

namespace Tbx
{
    struct TBX_API Quaternion
    {
    public:
        Quaternion() = default;
        Quaternion(float x, float y, float z, float w) : X(x), Y(y), Z(z), W(w) {}

        float X;
        float Y;
        float Z;
        float W;

        static Quaternion Identity() { return Quaternion(0.0f, 0.0f, 0.0f, 1.0f); }

        static Quaternion FromEuler(const Vector3& euler)
        {
            return FromEuler(euler.X, euler.Y, euler.Z);
        }

        static Quaternion FromEuler(float x, float y, float z)
        {
            float roll = x * 0.5f;
            float pitch = y * 0.5f;
            float yaw = z * 0.5f;

            float cr = Math::Cos(roll);
            float sr = Math::Sin(roll);
            float cp = Math::Cos(pitch);
            float sp = Math::Sin(pitch);
            float cy = Math::Cos(yaw);
            float sy = Math::Sin(yaw);

            return Quaternion(
                sr * cp * cy - cr * sp * sy, 
                cr * sp * cy + sr * cp * sy, 
                cr * cp * sy - sr * sp * cy, 
                cr * cp * cy + sr * sp * sy);
        }
    };
}