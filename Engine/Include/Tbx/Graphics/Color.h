#pragma once
#include "Tbx/Core/StringConvertible.h"
#include "Tbx/DllExport.h"
#include <string>

namespace Tbx
{
    struct EXPORT RgbaColor : public IStringConvertible
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

        std::string ToString() const override
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

        static RgbaColor White;
        static RgbaColor Black;
        static RgbaColor Red;
        static RgbaColor Green;
        static RgbaColor Blue;
        static RgbaColor Yellow;
        static RgbaColor Cyan;
        static RgbaColor Magenta;
        static RgbaColor Grey;
        static RgbaColor LightGrey;
        static RgbaColor DarkGrey;
    };
}