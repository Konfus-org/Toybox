#pragma once
#include "Tbx/Collections/LayerStack.h"
#include "Tbx/Windowing/WindowManager.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Events/EventListener.h"
#include <string>

namespace Tbx
{
    class WindowingLayer final : public Layer
    {
    public:
        WindowingLayer(
            const std::string& appName,
            Ref<IWindowFactory> windowFactory,
            Ref<EventBus> eventBus);

        void OnDetach() override;
        void OnUpdate() override;

    private:
        std::string _appName = "Toybox";
        ExclusiveRef<WindowManager> _windowManager = nullptr;
        EventListener _eventListener = {};
    };
}

