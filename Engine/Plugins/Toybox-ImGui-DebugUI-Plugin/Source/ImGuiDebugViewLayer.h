#pragma once
#include <Tbx/Application/Layers/Layer.h>
#include <Tbx/Systems/Rendering/RenderEvents.h>
#include <Tbx/Systems/Windowing/WindowEvents.h>
#include <Tbx/Utils/Ids/UID.h>
#include <nvrhi/nvrhi.h>
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

        nvrhi::IDevice* _graphicsDevice = nullptr;
        Tbx::Size _windowResolution = { 0, 0 };

        ProcessInfo _processInfo = {};
        SystemInformation _systemInfo = {};
    };
}