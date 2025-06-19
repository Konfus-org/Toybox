#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Type Aliases/Int.h"
#include <string>

namespace Tbx
{
    struct EXPORT Size
    {
    public:
        Size() = default;
        Size(int width, int height) : Width(width), Height(height) {}

        std::string ToString() const;
        float GetAspectRatio() const;

        uint Width = 0;
        uint Height = 0;
    };
}