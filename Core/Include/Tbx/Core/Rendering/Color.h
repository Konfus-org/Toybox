#pragma once
#include "Tbx/Core/DllExport.h"

namespace Tbx
{
    struct EXPORT Color
    {
    public:
        /// <summary>
        /// Default constructor, initializes the color to black (0, 0, 0, 1)
        /// </summary>
        Color() = default;

        /// <summary>
        /// Constructor that initializes the color with the given RGBA values.
        /// Given values are expected to be in the range of 0-1.
        /// </summary>
        Color(float r, float g, float b, float a) 
            : R(r), G(g), B(b), A(a) {}

        static Color White() { return Color(1.0f, 1.0f, 1.0f, 1.0f); }
        static Color Black() { return Color(0.0f, 0.0f, 0.0f, 1.0f); }
        static Color DarkGrey() { return Color(0.1f, 0.1f, 0.1f, 1.0f); }
        
        /// <summary>
        /// Amount of red in the color (0-1)
        /// </summary>
        float R;
        /// <summary>
        /// Amount of green in the color (0-1)
        /// </summary>
        float G;
        /// <summary>
        /// Amount of blue in the color (0-1)
        /// </summary>
        float B;
        /// <summary>
        /// Alpha/Transparency value (0-1)
        /// </summary>
        float A;
    };
    
}