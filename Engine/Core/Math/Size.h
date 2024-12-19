#pragma once
#include "Int.h"

namespace Toybox
{
    struct Size
    {
    public:
        Size() = default;
        Size(int width, int height) : Width(width), Height(height) {}

        uint Width;
        uint Height;
    };
}