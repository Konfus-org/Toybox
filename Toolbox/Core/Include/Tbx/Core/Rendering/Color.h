#pragma once
#include "Tbx/Core/DllExport.h"

namespace Tbx
{
    struct EXPORT Color
    {
    public:
        // Defaults to black
        Color() = default;

        //RGBA (0-1)
        Color(float r, float g, float b, float a) : R(r), G(g), B(b), A(a) {}

        float R; // Red 0-1
        float G; // Green 0-1
        float B; // Blue 0-1
        float A; // Alpha 0-1

        static Color White() { return Color(1.0f, 1.0f, 1.0f, 1.0f); } 
        static Color Black() { return Color(0.0f, 0.0f, 0.0f, 1.0f); }
        static Color DarkGrey() { return Color(0.1f, 0.1f, 0.1f, 1.0f); }
    };
    
}