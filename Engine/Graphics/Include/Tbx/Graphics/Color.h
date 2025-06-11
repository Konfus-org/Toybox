#pragma once
#include "Tbx/Utils/DllExport.h"

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

    namespace Colors
    {
        EXPORT inline const Color& White = Color(1.0f, 1.0f, 1.0f, 1.0f);
        EXPORT inline const Color& Black = Color(0.0f, 0.0f, 0.0f, 1.0f);
        EXPORT inline const Color& Red = Color(1.0f, 0.0f, 0.0f, 1.0f);
        EXPORT inline const Color& Green = Color(0.0f, 1.0f, 0.0f, 1.0f);
        EXPORT inline const Color& Blue = Color(0.0f, 0.0f, 1.0f, 1.0f);
        EXPORT inline const Color& Yellow = Color(1.0f, 1.0f, 0.0f, 1.0f);
        EXPORT inline const Color& Cyan = Color(0.0f, 1.0f, 1.0f, 1.0f);
        EXPORT inline const Color& Magenta = Color(1.0f, 0.0f, 1.0f, 1.0f);
        EXPORT inline const Color& Grey = Color(0.5f, 0.5f, 0.5f, 1.0f);
        EXPORT inline const Color& LightGrey = Color(0.75f, 0.75f, 0.75f, 1.0f);
        EXPORT inline const Color& DarkGrey = Color(0.1f, 0.1f, 0.1f, 1.0f);
    }
}