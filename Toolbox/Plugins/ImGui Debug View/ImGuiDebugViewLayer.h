#pragma once
#include <Tbx/App/Layers/Layer.h>
#include <Tbx/App/Events/RenderEvents.h>
#include <Tbx/Core/Ids/UID.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

namespace ImGuiDebugView
{
    class ImGuiDebugViewLayer : public Tbx::Layer
    {
    public:
        ImGuiDebugViewLayer() 
            : Tbx::Layer("ImGui") {}

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;
        bool IsOverlay() override;

    private:
        void OnFrameRendered(const Tbx::RenderedFrameEvent&) const;

        Tbx::UID _frameRenderedEventId = -1;
    };
}