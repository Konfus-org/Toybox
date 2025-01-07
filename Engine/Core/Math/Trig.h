#pragma once
#include "TbxAPI.h"

namespace Tbx::Math
{
    float TBX_API PI();

    float TBX_API DegreesToRadians(float degrees);
    float TBX_API RadiansToDegrees(float radians);

    float TBX_API Cos(float x);
    float TBX_API Sin(float x);
    float TBX_API Tan(float x);
    float TBX_API ACos(float x);
    float TBX_API ASin(float x);
    float TBX_API ATan(float x);
}
