#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/GraphicsApi.h"
#include <vector>

namespace Tbx
{
    /// <summary>
    /// A piece of the graphics pipeline
    /// </summary>
    class TBX_EXPORT IGraphicsPipe
    {
    public:
        virtual ~IGraphicsPipe() = 0;
        virtual std::vector<GraphicsApi> GetSupportedApis() const = 0;
    };
}