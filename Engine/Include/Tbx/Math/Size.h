#pragma once
#include "Tbx/Debug/IPrintable.h"
#include "Tbx/DllExport.h"
#include "Tbx/Math/Int.h"
#include <string>

namespace Tbx
{
    struct EXPORT Size : public IPrintable
    {
    public:
        Size() = default;
        Size(uint width, uint height) : Width(width), Height(height) {}
        Size(int width, int height) : Width(width), Height(height) {}

        std::string ToString() const override;
        float GetAspectRatio() const;

        uint Width = 0;
        uint Height = 0;
    };
}