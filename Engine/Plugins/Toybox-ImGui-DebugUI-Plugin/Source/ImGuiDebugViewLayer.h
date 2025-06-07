#pragma once
#include <Tbx/Application/Layers/Layer.h>
#include <Tbx/Application/Events/RenderEvents.h>
#include <Tbx/Application/Events/WindowEvents.h>
#include <Tbx/Core/Ids/UID.h>
#include <sys_info/ProcessInfo.h>
#include <sys_info/SystemInformation.h>

namespace ImGuiDebugView
{
    class ImGuiDebugViewLayer : public Tbx::Layer
    {
    public:
        ImGuiDebugViewLayer() : Layer("ImGui") {}

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