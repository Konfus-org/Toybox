#pragma once
#include "Tbx/Utils/DllExport.h"
#include "Tbx/Systems/Rendering/IRenderer.h"

namespace Tbx
{
    class EXPORT IRendererFactory
    {
    public:
        virtual ~IRendererFactory() = default;
        virtual std::shared_ptr<IRenderer> Create(std::shared_ptr<IRenderSurface> surface) = 0;
    };
}