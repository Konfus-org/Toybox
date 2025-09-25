#include "Tbx/PCH.h"
#include "Tbx/Layers/WindowingLayer.h"

namespace Tbx
{
    WindowingLayer::WindowingLayer(
        const std::string& appName,
        Ref<WindowManager> windowManager)
        : Layer("Windowing")
        , _windowManager(windowManager)
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
}

