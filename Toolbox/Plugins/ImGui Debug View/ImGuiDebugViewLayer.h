#pragma once
#include <Tbx/App/Layers/Layer.h>

namespace ImGuiDebugView
{
    class ImGuiDebugViewLayer : public Tbx::Layer
    {
    public:
        ImGuiDebugViewLayer() : Tbx::Layer("ImGui") {}
        ~ImGuiDebugViewLayer() override = default;

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;
        bool IsOverlay() override;
    };
}

