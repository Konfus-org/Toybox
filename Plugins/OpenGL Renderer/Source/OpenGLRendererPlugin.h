#pragma once
#include "OpenGLRenderer.h"
#include <Tbx/Core/Rendering/IRenderer.h>
#include <Tbx/Core/Plugins/RegisterPlugin.h>
#include <Tbx/Runtime/Events/RenderEvents.h>
#include <Tbx/Runtime/Events/WindowEvents.h>

namespace OpenGLRendering
{
    class OpenGLRendererPlugin : public Tbx::Plugin, public OpenGLRenderer
    {
    public:
        OpenGLRendererPlugin() = default;
        ~OpenGLRendererPlugin() final = default;

        void OnLoad() override;
        void OnUnload() override;

    private:
        void OnWindowFocusChanged(const Tbx::WindowFocusChangedEvent& e);
        void OnWindowResized(const Tbx::WindowResizedEvent& e);

        void OnSetVSyncEvent(Tbx::SetVSyncRequest& e);
        void OnRenderFrameEvent(Tbx::RenderFrameRequest& e);
        void OnClearScreenRenderEvent(Tbx::ClearScreenRequest& e);
        void OnFlushEvent(Tbx::FlushRendererRequest& e);

        Tbx::UID _windowFocusChangedEventId;
        Tbx::UID _windowResizedEventId;

        Tbx::UID _setVSyncEventId;
        Tbx::UID _renderFrameEventId;
        Tbx::UID _clearScreenEventId;
        Tbx::UID _flushEventId;
    };
}

TBX_REGISTER_PLUGIN(OpenGLRendering::OpenGLRendererPlugin);