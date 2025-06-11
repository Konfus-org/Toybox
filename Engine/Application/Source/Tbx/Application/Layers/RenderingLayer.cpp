#include "Tbx/Application/PCH.h"
#include "Tbx/Application/Layers/RenderingLayer.h"
#include "Tbx/Systems/Rendering/RenderPipeline.h"

namespace Tbx
{
    bool RenderingLayer::IsOverlay()
    {
        return false;
    }

    void RenderingLayer::OnAttach()
    {
        RenderPipeline::Initialize();
    }

    void RenderingLayer::OnUpdate()
    {
        RenderPipeline::Update();
    }

    void RenderingLayer::OnDetach()
    {
        RenderPipeline::Shutdown();
    }
}