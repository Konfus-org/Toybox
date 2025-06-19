#include "Tbx/PCH.h"
#include "Tbx/Layers/WindowingLayer.h"
#include "Tbx/Windowing/WindowManager.h"

namespace Tbx
{
    bool WindowingLayer::IsOverlay()
    {
        return false;
    }

    void WindowingLayer::OnAttach()
    {
        WindowManager::SetContext();
    }

    void WindowingLayer::OnUpdate()
    {
        WindowManager::DrawFrame();
    }

    void WindowingLayer::OnDetach()
    {
        WindowManager::Shutdown();
    }
}
