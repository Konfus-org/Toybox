#pragma once
#include <Tbx/Systems/Rendering/IRendererFactory.h>

namespace NVRHIRendering
{
    class NVRHIRendererFactory : public Tbx::IRendererFactory
    {
    public:
        std::shared_ptr<Tbx::IRenderer> Create(std::shared_ptr<Tbx::IRenderSurface> surface) override;
    };
}

