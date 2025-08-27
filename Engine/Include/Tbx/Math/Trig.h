#pragma once
#include "Tbx/DllExport.h"

namespace Tbx::Math
{
    EXPORT float DegreesToRadians(float degrees);
    EXPORT float RadiansToDegrees(float radians);

    EXPORT float Cos(float x);
    EXPORT float Sin(float x);
    EXPORT float Tan(float x);
    EXPORT float ACos(float x);
    EXPORT float ASin(float x);
    EXPORT float ATan(float x);

    EXPORT float Dot(float a, float b);
}
