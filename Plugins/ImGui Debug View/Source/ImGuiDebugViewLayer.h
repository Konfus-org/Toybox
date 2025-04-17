#pragma once
#include <Tbx/Runtime/Layers/Layer.h>
#include <Tbx/Runtime/Events/RenderEvents.h>
#include <Tbx/Runtime/Events/WindowEvents.h>
#include <Tbx/Core/Ids/UID.h>
#include <sys_info/ProcessInfo.h>
#include <sys_info/SystemInformation.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>
#include <imgui.h>
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
        void OnWindowResized(const Tbx::WindowResizedEvent& e);

        Tbx::UID _frameRenderedEventId = -1;
        Tbx::UID _windowResizedEventId = -1;

        bool _showDebugWindowOnDebugBtnUp = false;
        bool _isDebugWindowOpen = false;

        Tbx::Size _windowResolution = { 0, 0 };
        ProcessInfo _processInfo;
        SystemInformation _systemInfo;
    };
}