#pragma once
#include "ToolboxAPI.h"
#include "Int.h"

namespace Tbx
{
    struct TBX_API Size
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