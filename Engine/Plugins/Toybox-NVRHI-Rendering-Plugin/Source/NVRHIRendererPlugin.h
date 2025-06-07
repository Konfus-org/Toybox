#pragma once
#include "NVRHIRenderer.h"
#include <Tbx/Application/Events/RenderEvents.h>
#include <Tbx/Application/Events/WindowEvents.h>
#include <Tbx/Application/Events/ApplicationEvents.h>
#include <Tbx/Application/App/GraphicsSettings.h>
#include <Tbx/Core/Rendering/IRenderer.h>
#include <Tbx/Core/Plugins/RegisterPlugin.h>

namespace NVRHIRendering
{
    class NVRHIRendererPlugin : public Tbx::Plugin, public NVRHIRenderer
    {
    public:
        NVRHIRendererPlugin() = default;
        ~NVRHIRendererPlugin() final = default;

        void OnLoad() override;
        void OnUnload() override;

    private:
        void OnWindowFocusChanged(const Tbx::WindowFocusChangedEvent& e);
        void OnWindowResized(const Tbx::WindowResizedEvent& e);

        void OnGraphicsSettingsChanged(const Tbx::AppGraphicsSettingsChangedEvent& e);
        void OnRenderFrameRequest(Tbx::RenderFrameRequest& e);
        void OnClearScreenRequest(Tbx::ClearScreenRequest& e);
        void OnFlushRequest(Tbx::FlushRendererRequest& e);

        Tbx::GraphicsSettings _settings = {};
        Tbx::Color _clearColor = {};

        Tbx::UID _windowFocusChangedEventId = -1;
        Tbx::UID _windowResizedEventId = -1;

        Tbx::UID _graphicsSettingsChangedEventId = -1;

        Tbx::UID _setVSyncEventId = -1;
        Tbx::UID _renderFrameEventId = -1;
        Tbx::UID _clearScreenEventId = -1;
        Tbx::UID _flushEventId = -1;
    };
}

TBX_REGISTER_PLUGIN(NVRHIRendering::NVRHIRendererPlugin);