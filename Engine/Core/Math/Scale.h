#pragma once
#include "tbxAPI.h"

namespace Tbx
{
    struct TBX_API Scale
    {
    public:
        Scale() : X(1), Y(1), Z(1) {}
        Scale(float x, float y, float z) : X(x), Y(y), Z(z) {}

        float X;
        float Y;
        float Z;

        static Scale Identity() { return Scale(1.0f, 1.0f, 1.0f); }

        Matrix ToMatrix() const
        {
            return Matrix({
                X, 0.0f, 0.0f, 0.0f,
                0.0f, Y, 0.0f, 0.0f,
                0.0f, 0.0f, Z, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
            });
        }
    };
}
