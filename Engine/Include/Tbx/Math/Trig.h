#pragma once
#include "Tbx/DllExport.h"

namespace Tbx
{
    // TODO: Pull static 'Add, Multiply, Divide, etc..' functions from various math classes and implement them here!
    inline const float PI = 3.14159265358979323846264338327950288f;

    TBX_EXPORT float DegreesToRadians(float degrees);
    TBX_EXPORT float RadiansToDegrees(float radians);

    TBX_EXPORT float Cos(float x);
    TBX_EXPORT float Sin(float x);
    TBX_EXPORT float Tan(float x);
    TBX_EXPORT float ACos(float x);
    TBX_EXPORT float ASin(float x);
    TBX_EXPORT float ATan(float x);
}
