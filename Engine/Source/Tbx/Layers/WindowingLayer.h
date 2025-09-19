#pragma once
#include "Tbx/Layers/Layer.h"
#include "Tbx/Math/Size.h"
#include "Tbx/Windowing/WindowManager.h"
#include "Tbx/Memory/Refs.h"
#include <memory>
#include <string>

namespace Tbx
{
    class WindowingLayer : public Layer
    {
    public:
        WindowingLayer(
            const std::string& appName,
            Tbx::Ref<IWindowFactory> windowFactory,
            Tbx::Ref<EventBus> eventBus);

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;

        Tbx::Ref<WindowManager> GetWindowManager() const;

    private:
        std::string _appName = "Toybox";
        Tbx::Ref<WindowManager> _windowManager = nullptr;
    };
}

