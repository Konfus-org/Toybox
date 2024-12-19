#pragma once
#include "tbxpch.h"
#include "int.h"

namespace Toybox
{
    struct Size
    {
    public:
        Size() : Width(0), Height(0) {}
        Size(int width, int height) : Width(width), Height(height) {}

        uint Width;
        uint Height;
    };
}