#include "NVRHIRendererFactory.h"
#include "NVRHIRenderer.h"

namespace NVRHIRendering
{
    Tbx::IRenderer* CreateRenderer(std::shared_ptr<Tbx::IRenderSurface> surface)
    {
        // Create an NVRHIRenderer and initialize it with the provided surface
        auto* renderer = new NVRHIRenderer();
        renderer->Initialize(surface);
        return renderer;
    }

    void DeleteRenderer(Tbx::IRenderer* renderer)
    {
        delete renderer;
    }

    std::shared_ptr<Tbx::IRenderer> NVRHIRendererFactory::Create(std::shared_ptr<Tbx::IRenderSurface> surface)
    {
        return std::shared_ptr<Tbx::IRenderer>(CreateRenderer(surface), [](Tbx::IRenderer* renderer) 
        { 
            DeleteRenderer(renderer); 
        });
    }
}
