#pragma once
#include "Tbx/Layers/Layer.h"
#include "Tbx/Windowing/WindowManager.h"
#include "Tbx/Memory/Refs.h"
#include <string>

namespace Tbx
{
    class WindowingLayer : public Layer
    {
    public:
        WindowingLayer(
            const std::string& appName,
            Ref<WindowManager> windowManager);

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;

    private:
        std::string _appName = "Toybox";
        Ref<WindowManager> _windowManager = nullptr;
    };
}

