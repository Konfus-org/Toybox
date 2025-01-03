#pragma once
#include "Int.h"
#include "tbxAPI.h"

namespace Tbx
{
    struct TBX_API Size
    {
    public:
        Size() = default;
        Size(int width, int height) : Width(width), Height(height) {}

        uint Width;
        uint Height;
    };
}