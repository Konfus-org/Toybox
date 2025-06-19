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

    void WindowingLayer::OnDetach()
    {
        WindowManager::Shutdown();
    }

    void WindowingLayer::OnUpdate()
    {
        WindowManager::Update();
    }
}
