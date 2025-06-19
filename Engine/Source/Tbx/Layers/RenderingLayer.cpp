#include "Tbx/PCH.h"
#include "Tbx/Layers/RenderingLayer.h"
#include "Tbx/Graphics/Rendering.h"

namespace Tbx
{
    bool RenderingLayer::IsOverlay()
    {
        return false;
    }

    void RenderingLayer::OnAttach()
    {
        Rendering::Initialize();
    }

    void RenderingLayer::OnDetach()
    {
        Rendering::Shutdown();
    }

    void RenderingLayer::OnUpdate()
    {
        Rendering::DrawFrame();
    }
}