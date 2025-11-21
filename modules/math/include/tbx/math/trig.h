#pragma once
#include "tbx/tbx_api.h"

namespace tbx
{
    inline const float PI = 3.14159265358979323846264338327950288f;

    TBX_API float degrees_to_radians(float degrees);
    TBX_API float radians_to_degrees(float radians);
    TBX_API float cos(float x);
    TBX_API float sin(float x);
    TBX_API float tan(float x);
    TBX_API float acos(float x);
    TBX_API float asin(float x);
    TBX_API float atan(float x);
}
