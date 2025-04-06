#include "OpenGLRendererPlugin.h"
#include <Tbx/Core/Events/EventCoordinator.h>
#include <Tbx/App/Render Pipeline/RenderPipeline.h>
#include <Tbx/App/Windowing/WindowManager.h>

namespace OpenGLRendering
{
    void OpenGLRendererPlugin::OnLoad()
    {
        _windowFocusChangedEventId = 
            Tbx::EventCoordinator::Subscribe<Tbx::WindowFocusChanged>(TBX_BIND_FN(OnWindowFocusChanged));
        _windowResizedEventId = 
            Tbx::EventCoordinator::Subscribe<Tbx::WindowResized>(TBX_BIND_FN(OnWindowResized));

        _setVSyncEventId = 
            Tbx::EventCoordinator::Subscribe<Tbx::SetVSyncRequest>(TBX_BIND_FN(OnSetVSyncEvent));
        _renderFrameEventId = 
            Tbx::EventCoordinator::Subscribe<Tbx::RenderFrameRequest>(TBX_BIND_FN(OnRenderFrameEvent));
        _clearScreenEventId = 
            Tbx::EventCoordinator::Subscribe<Tbx::ClearScreenRequest>(TBX_BIND_FN(OnClearScreenRenderEvent));
        _flushEventId = 
            Tbx::EventCoordinator::Subscribe<Tbx::FlushRendererRequest>(TBX_BIND_FN(OnFlushEvent));
    }

    void OpenGLRendererPlugin::OnUnload()
    {
        Tbx::EventCoordinator::Unsubscribe<Tbx::WindowFocusChanged>(_windowFocusChangedEventId);
        Tbx::EventCoordinator::Unsubscribe<Tbx::WindowResized>(_windowResizedEventId);

        Tbx::EventCoordinator::Unsubscribe<Tbx::SetVSyncRequest>(_setVSyncEventId);
        Tbx::EventCoordinator::Unsubscribe<Tbx::RenderFrameRequest>(_renderFrameEventId);
        Tbx::EventCoordinator::Unsubscribe<Tbx::ClearScreenRequest>(_clearScreenEventId);
        Tbx::EventCoordinator::Unsubscribe<Tbx::FlushRendererRequest>(_flushEventId);
    }

    void OpenGLRendererPlugin::OnWindowFocusChanged(const Tbx::WindowFocusChanged& e)
    {
        if (!e.IsFocused()) return;

        SetContext(Tbx::WindowManager::GetWindow(e.GetWindowId()));
        SetViewport({ 0, 0 }, Tbx::WindowManager::GetWindow(e.GetWindowId()).lock()->GetSize());
    }

    void OpenGLRendererPlugin::OnWindowResized(const Tbx::WindowResized& e)
    {
        std::weak_ptr<Tbx::IWindow> windowThatWasResized = Tbx::WindowManager::GetWindow(e.GetWindowId());

        SetVSyncEnabled(true); // Enable vsync so the window doesn't flicker

        // Draw the window while its resizing so there are no artifacts during the resize
        const bool& wasVSyncEnabled = Tbx::RenderPipeline::IsVSyncEnabled();
        SetViewport({ 0, 0 }, e.GetNewSize());
        Redraw();
        SetVSyncEnabled(wasVSyncEnabled); // Set vsync back to what it was

        // Log window resize
        const auto& newSize = windowThatWasResized.lock()->GetSize();
        const auto& name = windowThatWasResized.lock()->GetTitle();
        TBX_INFO("Renderer responding to Window {0} resized to {1}x{2}", name, newSize.Width, newSize.Height);
    }

    void OpenGLRendererPlugin::OnSetVSyncEvent(Tbx::SetVSyncRequest& e)
    {
        SetVSyncEnabled(e.GetVSync());
        e.IsHandled = true;
    }

    void OpenGLRendererPlugin::OnRenderFrameEvent(Tbx::RenderFrameRequest& e)
    {
        BeginDraw();

        const auto& batch = e.GetBatch();
        for (const auto& item : batch)
        {
            ProcessData(item);
        }
        e.IsHandled = true;

        EndDraw();
    }

    void OpenGLRendererPlugin::OnClearScreenRenderEvent(Tbx::ClearScreenRequest& e)
    {
        Clear(Tbx::Color::DarkGrey());
        e.IsHandled = true;
    }

    void OpenGLRendererPlugin::OnFlushEvent(Tbx::FlushRendererRequest& e)
    {
        Flush();
        e.IsHandled = true;
    }
}

TBX_REGISTER_PLUGIN(OpenGLRendering::OpenGLRendererPlugin);