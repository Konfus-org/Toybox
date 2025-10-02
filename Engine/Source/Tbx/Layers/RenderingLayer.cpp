#include "Tbx/PCH.h"
#include "Tbx/Layers/RenderingLayer.h"
#include <memory>

namespace Tbx
{
    RenderingLayer::RenderingLayer(
        const std::vector<Ref<IRendererFactory>>& renderFactories,
        const std::vector<Ref<IGraphicsContextProvider>>& graphicsContextProviders,
        Ref<EventBus> eventBus)
        : Layer("Rendering")
    {
        _rendering = MakeExclusive<RenderingPipeline>(renderFactories, graphicsContextProviders, eventBus);
    }

    void RenderingLayer::OnUpdate()
    {
        _rendering->Update();
    }
}

