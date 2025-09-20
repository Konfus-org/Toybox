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

    Ref<IRenderer> RenderingLayer::GetRenderer(const Ref<IWindow>& window)
    {
        return _rendering->GetRenderer(window);
    }

    void RenderingLayer::OnUpdate()
    {
        _rendering->Update();
    }
}

