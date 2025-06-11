#include "Tbx/Application/PCH.h"
#include "Tbx/Application/Layers/WindowingLayer.h"
#include "Tbx/Systems/Windowing/WindowManager.h"

namespace Tbx
{
    bool WindowingLayer::IsOverlay()
    {
        return false;
    }

    void WindowingLayer::OnAttach()
    {
        WindowManager::Initialize();
    }

    void WindowingLayer::OnUpdate()
    {
        WindowManager::Update();
    }

    void WindowingLayer::OnDetach()
    {
        WindowManager::Shutdown();
    }
}
