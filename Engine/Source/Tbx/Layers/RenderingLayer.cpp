#include "Tbx/PCH.h"
#include "Tbx/Layers/RenderingLayer.h"
#include <memory>

namespace Tbx
{
    RenderingLayer::RenderingLayer(Ref<IRendererFactory> renderFactory, Ref<EventBus> eventBus)
        : Layer("Rendering")
    {
        _rendering = std::make_unique<Rendering>(renderFactory, eventBus);
    }

    void RenderingLayer::OnUpdate()
    {
        _rendering->Update();
    }
}

