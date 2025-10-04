#include "Tbx/PCH.h"
#include "Tbx/App/Layers/RenderingLayer.h"
#include <memory>

namespace Tbx
{
    RenderingLayer::RenderingLayer(
        const std::vector<Ref<IManageGraphicsApis>>& apiManagers,
        const std::vector<Ref<IGraphicsResourceFactory>>& resourceFactories,
        const std::vector<Ref<IGraphicsContextProvider>>& contextProviders,
        const std::vector<Ref<IShaderCompiler>>& shaderCompilers,
        Ref<EventBus> eventBus)
        : Layer("Rendering")
    {
        _pipeline = MakeExclusive<GraphicsPipeline>(apiManagers, resourceFactories, contextProviders, shaderCompilers, eventBus);
    }

    void RenderingLayer::OnUpdate()
    {
        _pipeline->Update();
    }
}

