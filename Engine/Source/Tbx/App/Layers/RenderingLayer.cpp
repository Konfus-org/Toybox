#include "Tbx/PCH.h"
#include "Tbx/App/Layers/RenderingLayer.h"
#include <memory>

namespace Tbx
{
    RenderingLayer::RenderingLayer(
        const std::vector<Ref<IGraphicsBackend>>& backends,
        const std::vector<Ref<IGraphicsContextProvider>>& contextProviders,
        Ref<EventBus> eventBus)
        : Layer("Rendering")
    {
        _pipeline = MakeExclusive<GraphicsPipeline>(backends, contextProviders, eventBus);
    }

    void RenderingLayer::OnUpdate()
    {
        _pipeline->Update();
    }
}

