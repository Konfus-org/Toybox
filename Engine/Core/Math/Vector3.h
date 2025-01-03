#pragma once
#include "TbxPCH.h"
#include "TbxAPI.h"
#include "Matrix.h"

namespace Tbx
{
    struct TBX_API Vector3
    {
    public:
        Vector3() = default;
        Vector3(float x, float y, float z) : X(x), Y(y), Z(z) {}

        float X;
        float Y;
        float Z;

        static Vector3 Identity() { return Vector3(1, 1, 1); }

        Matrix ToMatrix() const
        {
            return Matrix({
                X, 0, 0, 0,
                0, Y, 0, 0,
                0, 0, Z, 0,
                0, 0, 0, 1
            });
        }
    };
}
