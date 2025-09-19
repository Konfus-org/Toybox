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
        WindowingLayer(
            const std::string& appName,
            std::shared_ptr<IWindowFactory> windowFactory,
            std::shared_ptr<EventBus> eventBus);

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;

        std::shared_ptr<WindowManager> GetWindowManager() const;

    private:
        std::string _appName = "Toybox";
        std::shared_ptr<WindowManager> _windowManager = nullptr;
    };
}

