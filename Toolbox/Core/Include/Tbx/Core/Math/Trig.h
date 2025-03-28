#pragma once
#include "Tbx/Core/DllExport.h"

namespace Tbx::Math
{
    float EXPORT PI();

    float EXPORT DegreesToRadians(float degrees);
    float EXPORT RadiansToDegrees(float radians);

    float EXPORT Cos(float x);
    float EXPORT Sin(float x);
    float EXPORT Tan(float x);
    float EXPORT ACos(float x);
    float EXPORT ASin(float x);
    float EXPORT ATan(float x);
}
