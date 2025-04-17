#include "OpenGLRendererPlugin.h"
#include <Tbx/Core/Events/EventCoordinator.h>
#include <Tbx/Runtime/Render Pipeline/RenderPipeline.h>
#include <Tbx/Runtime/Windowing/WindowManager.h>

namespace OpenGLRendering
{
    void OpenGLRendererPlugin::OnLoad()
    {
        _windowFocusChangedEventId = 
            Tbx::EventCoordinator::Subscribe<Tbx::WindowFocusChangedEvent>(TBX_BIND_FN(OnWindowFocusChanged));
        _windowResizedEventId = 
            Tbx::EventCoordinator::Subscribe<Tbx::WindowResizedEvent>(TBX_BIND_FN(OnWindowResized));

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
        Tbx::EventCoordinator::Unsubscribe<Tbx::WindowFocusChangedEvent>(_windowFocusChangedEventId);
        Tbx::EventCoordinator::Unsubscribe<Tbx::WindowResizedEvent>(_windowResizedEventId);

        Tbx::EventCoordinator::Unsubscribe<Tbx::SetVSyncRequest>(_setVSyncEventId);
        Tbx::EventCoordinator::Unsubscribe<Tbx::RenderFrameRequest>(_renderFrameEventId);
        Tbx::EventCoordinator::Unsubscribe<Tbx::ClearScreenRequest>(_clearScreenEventId);
        Tbx::EventCoordinator::Unsubscribe<Tbx::FlushRendererRequest>(_flushEventId);
    }

    void OpenGLRendererPlugin::OnWindowFocusChanged(const Tbx::WindowFocusChangedEvent& e)
    {
        if (!e.IsFocused()) return;

        SetContext(Tbx::WindowManager::GetWindow(e.GetWindowId()));
        SetViewport({ 0, 0 }, Tbx::WindowManager::GetWindow(e.GetWindowId()).lock()->GetSize());
    }

    void OpenGLRendererPlugin::OnWindowResized(const Tbx::WindowResizedEvent& e)
    {
        std::weak_ptr<Tbx::IWindow> windowThatWasResized = Tbx::WindowManager::GetWindow(e.GetWindowId());
        
        // Enable vsync so the window doesn't flicker
        const bool& wasVSyncEnabled = Tbx::RenderPipeline::IsVSyncEnabled();
        SetVSyncEnabled(true);

        // Draw the window while its resizing so there are no artifacts during the resize
        SetViewport({ 0, 0 }, e.GetNewSize());
        Clear(Tbx::Color::DarkGrey());
        BeginDraw();
        Redraw();
        EndDraw();

        // Set vsync back to what it was
        SetVSyncEnabled(wasVSyncEnabled); 

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
        Clear(Tbx::Color::DarkGrey());

        BeginDraw();

        const auto& batch = e.GetBatch();
        for (const auto& item : batch)
        {
            ProcessData(item);
        }
        e.IsHandled = true;

        auto renderedFrameEvent = Tbx::RenderedFrameEvent();
        Tbx::EventCoordinator::Send<Tbx::RenderedFrameEvent>(renderedFrameEvent);

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