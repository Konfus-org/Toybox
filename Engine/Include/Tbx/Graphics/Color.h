#pragma once
#include "Tbx/DllExport.h"
#include <string>

namespace Tbx
{
    struct EXPORT RgbaColor
    {
    public:
        /// <summary>
        /// Default constructor, initializes the color to black (0, 0, 0, 1)
        /// </summary>
        RgbaColor() = default;

        /// <summary>
        /// Constructor that initializes the color with the given RGBA values.
        /// Given values are expected to be in the range of 0-1.
        /// </summary>
        RgbaColor(float r, float g, float b, float a)
            : R(r), G(g), B(b), A(a) {}

        std::string ToString() const
        {
            return
                "R: " + std::to_string(R) + 
                "G: " + std::to_string(G) + 
                "B: " + std::to_string(B) + 
                "A: " + std::to_string(A);
        }
        
        /// <summary>
        /// Amount of red in the color (0-1)
        /// </summary>
        float R = 0;
        /// <summary>
        /// Amount of green in the color (0-1)
        /// </summary>
        float G = 0;
        /// <summary>
        /// Amount of blue in the color (0-1)
        /// </summary>
        float B = 0;
        /// <summary>
        /// Alpha/Transparency value (0-1)
        /// </summary>
        float A = 1;
    };

    namespace Consts::Colors
    {
        EXPORT inline const RgbaColor& White = RgbaColor(1.0f, 1.0f, 1.0f, 1.0f);
        EXPORT inline const RgbaColor& Black = RgbaColor(0.0f, 0.0f, 0.0f, 1.0f);
        EXPORT inline const RgbaColor& Red = RgbaColor(1.0f, 0.0f, 0.0f, 1.0f);
        EXPORT inline const RgbaColor& Green = RgbaColor(0.0f, 1.0f, 0.0f, 1.0f);
        EXPORT inline const RgbaColor& Blue = RgbaColor(0.0f, 0.0f, 1.0f, 1.0f);
        EXPORT inline const RgbaColor& Yellow = RgbaColor(1.0f, 1.0f, 0.0f, 1.0f);
        EXPORT inline const RgbaColor& Cyan = RgbaColor(0.0f, 1.0f, 1.0f, 1.0f);
        EXPORT inline const RgbaColor& Magenta = RgbaColor(1.0f, 0.0f, 1.0f, 1.0f);
        EXPORT inline const RgbaColor& Grey = RgbaColor(0.5f, 0.5f, 0.5f, 1.0f);
        EXPORT inline const RgbaColor& LightGrey = RgbaColor(0.75f, 0.75f, 0.75f, 1.0f);
        EXPORT inline const RgbaColor& DarkGrey = RgbaColor(0.1f, 0.1f, 0.1f, 1.0f);
    }
}