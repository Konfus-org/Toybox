#pragma once
#include "Tbx/Layers/Layer.h"
#include "Tbx/Math/Size.h"
#include "Tbx/Windowing/WindowManager.h"
#include <memory>
#include <string>

namespace Tbx
{
    class WindowingLayer : public Layer
    {
    public:
        WindowingLayer(std::shared_ptr<IWindowFactory> windowFactory,
                       std::shared_ptr<EventBus> eventBus,
                       std::string mainWindowTitle,
                       WindowMode mainWindowMode,
                       Size mainWindowSize);

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;

        std::shared_ptr<WindowManager> GetWindowManager() const;

    private:
        std::shared_ptr<WindowManager> _windowManager;
        std::string _mainWindowTitle;
        WindowMode _mainWindowMode;
        Size _mainWindowSize;
        bool _shouldCreateMainWindow = false;
    };
}

