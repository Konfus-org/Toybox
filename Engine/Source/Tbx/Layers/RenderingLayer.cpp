#include "Tbx/PCH.h"
#include "Tbx/Layers/RenderingLayer.h"
#include <memory>

namespace Tbx
{
    RenderingLayer::RenderingLayer(Tbx::Ref<IRendererFactory> renderFactory, Tbx::Ref<EventBus> eventBus)
        : Layer("Rendering")
    {
        _rendering = std::make_unique<Rendering>(renderFactory, eventBus);
    }

    Tbx::Ref<IRenderer> RenderingLayer::GetRenderer(const Tbx::Ref<IWindow>& window)
    {
        if (!_rendering)
        {
            return nullptr;
        }

        return _rendering->GetRenderer(window);
    }

    void RenderingLayer::OnUpdate()
    {
        if (_rendering)
        {
            _rendering->Update();
        }
    }
}

