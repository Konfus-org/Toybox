#include "Tbx/PCH.h"
#include "Tbx/App/Layers/WindowingLayer.h"
#include "Tbx/Events/AppEvents.h"

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
        _eventListener.Bind(eventBus);
        _eventListener.Listen<AppLaunchedEvent>([this](const AppLaunchedEvent& e)
        {
#ifdef TBX_DEBUG
            _windowManager->OpenWindow(_appName, WindowMode::Windowed, Size(1920, 1080));
#else
            _windowManager->OpenWindow(_appName, WindowMode::Fullscreen);
#endif
        });
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

