#pragma once
#include "Tbx/DllExport.h"
#include <functional>
#include <string>

namespace Tbx
{
    struct Material;

    using RenderPassFilter = std::function<bool(const Material&)>;

    struct TBX_EXPORT RenderPass
    {
        std::string Name = {};
        bool DepthTestEnabled = true;
        RenderPassFilter Filter = nullptr;
    };
}
