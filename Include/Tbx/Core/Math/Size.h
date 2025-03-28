#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Math/Int.h"

namespace Tbx
{
    struct EXPORT Size
    {
    public:
        Size() = default;
        Size(int width, int height) : Width(width), Height(height) {}

        uint Width;
        uint Height;

        std::string ToString() const { return std::format("(Width: {}, Height: {})", Width, Height); }
        float AspectRatio() const { return (float)Width / (float)Height; }
    };
}