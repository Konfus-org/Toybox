#pragma once
#include "tbxapi.h"
#include "Int.h"

namespace Toybox
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