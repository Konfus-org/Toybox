#include "Tbx/Application/PCH.h"
#include "Tbx/Application/Layers/RenderingLayer.h"
#include "Tbx/Systems/Rendering/Rendering.h"

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

    void RenderingLayer::OnUpdate()
    {
        Rendering::DrawFrame();
    }

    void RenderingLayer::OnDetach()
    {
        Rendering::Shutdown();
    }
}