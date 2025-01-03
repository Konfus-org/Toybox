#pragma once
#include "tbxAPI.h"

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

        Matrix ToMatrix() const
        {
            float xx = X * X;
            float xy = X * Y;
            float xz = X * Z;
            float xw = X * W;
            float yy = Y * Y;
            float yz = Y * Z;
            float yw = Y * W;
            float zz = Z * Z;
            float zw = Z * W;
            return Matrix({
                1 - 2 * (yy + zz), 2 * (xy - zw), 2 * (xz + yw), 0,
                2 * (xy + zw), 1 - 2 * (xx + zz), 2 * (yz - xw), 0,
                2 * (xz - yw), 2 * (yz + xw), 1 - 2 * (xx + yy), 0,
                0, 0, 0, 1
            });
        }
    };
}