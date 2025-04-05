#pragma once
#include "OpenGLRenderer.h"
#include <Tbx/Core/Rendering/IRenderer.h>
#include <Tbx/Core/Plugins/RegisterPlugin.h>
#include <Tbx/App/Events/RenderEvents.h>

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
        void OnSetRenderContextEvent(Tbx::SetRenderContextRequestEvent& e);
        void OnSetVSyncEvent(Tbx::SetVSyncRequestEvent& e);

        void OnBeginRenderFrameEvent(Tbx::BeginRenderFrameRequestEvent& e);
        void OnRenderFrameEvent(Tbx::RenderFrameRequestEvent& e);
        void OnEndRenderFrameEvent(Tbx::EndRenderFrameRequestEvent& e);

        void OnClearScreenRenderEvent(Tbx::ClearScreenRequestEvent& e);
        void OnFlushEvent(Tbx::FlushRendererRequestEvent& e);

        Tbx::UID _setRenderContextEventId;
        Tbx::UID _setVSyncEventId;
        Tbx::UID _beginRenderFrameEventId;
        Tbx::UID _endRenderFrameEventId;
        Tbx::UID _renderFrameEventId;
        Tbx::UID _clearScreenEventId;
        Tbx::UID _flushEventId;
    };
}