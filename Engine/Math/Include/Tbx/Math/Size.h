#pragma once
#include "Tbx/Math/DllExport.h"
#include "Tbx/Math/Int.h"
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