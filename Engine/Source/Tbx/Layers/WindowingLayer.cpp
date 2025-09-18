#include "Tbx/PCH.h"
#include "Tbx/Layers/WindowingLayer.h"

namespace Tbx
{
    WindowingLayer::WindowingLayer(std::shared_ptr<IWindowFactory> windowFactory,
                                   std::shared_ptr<EventBus> eventBus,
                                   std::string mainWindowTitle,
                                   WindowMode mainWindowMode,
                                   Size mainWindowSize)
        : Layer("Windowing"),
          _mainWindowTitle(std::move(mainWindowTitle)),
          _mainWindowMode(mainWindowMode),
          _mainWindowSize(mainWindowSize)
    {
        _windowManager = std::make_shared<WindowManager>(windowFactory, eventBus);
        _shouldCreateMainWindow = !_mainWindowTitle.empty();
    }

    void WindowingLayer::OnAttach()
    {
        if (_windowManager && _shouldCreateMainWindow)
        {
            _windowManager->OpenWindow(_mainWindowTitle, _mainWindowMode, _mainWindowSize);
            _shouldCreateMainWindow = false;
        }
    }

    void WindowingLayer::OnDetach()
    {
        if (_windowManager)
        {
            _windowManager->CloseAllWindows();
        }
        _shouldCreateMainWindow = !_mainWindowTitle.empty();
    }

    void WindowingLayer::OnUpdate()
    {
        if (_windowManager)
        {
            _windowManager->UpdateWindows();
        }
    }

    std::shared_ptr<WindowManager> WindowingLayer::GetWindowManager() const
    {
        return _windowManager;
    }
}

