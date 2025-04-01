#pragma once
#include <Tbx/App/Layers/Layer.h>

namespace ImGuiDebugViewPlugin
{
    class ImGuiDebugViewLayer : public Tbx::Layer
    {
        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;
    };
}

