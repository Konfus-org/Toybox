#pragma once
#include "tbxAPI.h"

namespace Tbx
{
    struct TBX_API Color
    {
    public:
        Color() = default;
        Color(float r, float g, float b, float a) : R(r), G(g), B(b), A(a) {}

        float R; // Red 0-1
        float G; // Green 0-1
        float B; // Blue 0-1
        float A; // Alpha 0-1
    };
    
}