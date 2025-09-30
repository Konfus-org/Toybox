#include "Tbx/PCH.h"
#include "Tbx/Layers/WindowingLayer.h"

namespace Tbx
{
    WindowingLayer::WindowingLayer(
        const std::string& appName,
        Ref<IWindowFactory> windowFactory,
        Ref<EventBus> eventBus)
        : Layer("Windowing")
        , _appName(appName)
        , _windowManager(MakeExclusive<WindowManager>(windowFactory, eventBus))
    {
    }

    void WindowingLayer::OnAttach()
    {
#ifdef TBX_DEBUG
        _windowManager->OpenWindow(_appName, WindowMode::Windowed, Size(1920, 1080));
#else
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
}

