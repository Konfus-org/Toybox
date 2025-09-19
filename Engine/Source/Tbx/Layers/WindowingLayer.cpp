#include "Tbx/PCH.h"
#include "Tbx/Layers/WindowingLayer.h"

namespace Tbx
{
    WindowingLayer::WindowingLayer(const std::string& appName, Tbx::Ref<IWindowFactory> windowFactory, Tbx::Ref<EventBus> eventBus) 
        : Layer("Windowing")
        , _windowManager(std::make_shared<WindowManager>(windowFactory, eventBus))
        , _appName(appName)
    {
    }

    void WindowingLayer::OnAttach()
    {
#ifdef TBX_DEBUG
        _windowManager->OpenWindow(_appName, WindowMode::Windowed, Size(1920, 1080));
#elif
        _windowManager->OpenWindow(_appName, WindowMode::Fullscreen);
#endif
    }

    void WindowingLayer::OnDetach()
    {
        _windowManager->CloseAllWindows();
    }

    void WindowingLayer::OnUpdate()
    {
        _windowManager->UpdateWindows();
    }

    Tbx::Ref<WindowManager> WindowingLayer::GetWindowManager() const
    {
        return _windowManager;
    }
}

