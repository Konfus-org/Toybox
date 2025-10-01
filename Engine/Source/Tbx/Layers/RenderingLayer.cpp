#include "Tbx/PCH.h"
#include "Tbx/Layers/RenderingLayer.h"
#include <memory>

namespace Tbx
{
    RenderingLayer::RenderingLayer(
        const std::vector<Ref<IRendererFactory>>& renderFactories,
        const std::vector<Ref<IGraphicsConfigProvider>>& graphicsContextProviders,
        Ref<EventBus> eventBus)
        : Layer("Rendering")
    {
        _rendering = MakeExclusive<Rendering>(renderFactories, graphicsContextProviders, eventBus);
    }

    void RenderingLayer::OnUpdate()
    {
        _rendering->Update();
    }
}

