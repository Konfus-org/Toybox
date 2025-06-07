#include "NVRHIRendererPlugin.h"
#include <Tbx/Core/Events/EventCoordinator.h>
#include <Tbx/Application/Windowing/WindowManager.h>

namespace NVRHIRendering
{
    void NVRHIRendererPlugin::OnLoad()
    {
        _windowFocusChangedEventId = 
            Tbx::EventCoordinator::Subscribe<Tbx::WindowFocusChangedEvent>(TBX_BIND_FN(OnWindowFocusChanged));
        _windowResizedEventId = 
            Tbx::EventCoordinator::Subscribe<Tbx::WindowResizedEvent>(TBX_BIND_FN(OnWindowResized));

        _graphicsSettingsChangedEventId =
            Tbx::EventCoordinator::Subscribe<Tbx::AppGraphicsSettingsChangedEvent>(TBX_BIND_FN(OnGraphicsSettingsChanged));

        _renderFrameEventId = 
            Tbx::EventCoordinator::Subscribe<Tbx::RenderFrameRequest>(TBX_BIND_FN(OnRenderFrameRequest));
        _clearScreenEventId = 
            Tbx::EventCoordinator::Subscribe<Tbx::ClearScreenRequest>(TBX_BIND_FN(OnClearScreenRequest));
        _flushEventId = 
            Tbx::EventCoordinator::Subscribe<Tbx::FlushRendererRequest>(TBX_BIND_FN(OnFlushRequest));
    }

    void NVRHIRendererPlugin::OnUnload()
    {
        Tbx::EventCoordinator::Unsubscribe<Tbx::WindowFocusChangedEvent>(_windowFocusChangedEventId);
        Tbx::EventCoordinator::Unsubscribe<Tbx::WindowResizedEvent>(_windowResizedEventId);

        Tbx::EventCoordinator::Unsubscribe<Tbx::WindowResizedEvent>(_graphicsSettingsChangedEventId);

        Tbx::EventCoordinator::Unsubscribe<Tbx::RenderFrameRequest>(_renderFrameEventId);
        Tbx::EventCoordinator::Unsubscribe<Tbx::ClearScreenRequest>(_clearScreenEventId);
        Tbx::EventCoordinator::Unsubscribe<Tbx::FlushRendererRequest>(_flushEventId);

        Flush();
    }

    void NVRHIRendererPlugin::OnWindowResized(const Tbx::WindowResizedEvent& e)
    {
        std::weak_ptr<Tbx::IWindow> windowThatWasResized = Tbx::WindowManager::GetWindow(e.GetWindowId());
        if (windowThatWasResized != GetContext())
        
        // Enable vsync so the window doesn't flicker
        SetVSyncEnabled(true);

        // Draw the window while its resizing so there are no artifacts during the resize
        SetViewport(Tbx::ViewPort{ e.GetNewSize(), {0, 0} });
        Clear(_clearColor);
        BeginDraw();
        EndDraw();

        // Set vsync back to what it was
        SetVSyncEnabled(_settings.VSyncEnabled);
    }

    void NVRHIRendererPlugin::OnGraphicsSettingsChanged(const Tbx::AppGraphicsSettingsChangedEvent& e)
    {
        const auto& settings = e.GetNewSettings();

        _settings = settings;
        _clearColor = settings.ClearColor;
        SetVSyncEnabled(settings.VSyncEnabled);
        SetResolution(settings.Resolution);
    }

    void NVRHIRendererPlugin::OnRenderFrameRequest(Tbx::RenderFrameRequest& e)
    {
        Flush();
        Clear(_clearColor);

        BeginDraw();

        const auto& batch = e.GetBatch();
        for (const auto& item : batch)
        {
            RenderFrame(item);
        }

        auto renderedFrameEvent = Tbx::RenderedFrameEvent();
        Tbx::EventCoordinator::Send<Tbx::RenderedFrameEvent>(renderedFrameEvent);

        EndDraw();

        e.IsHandled = true;
    }

    void NVRHIRendererPlugin::OnClearScreenRequest(Tbx::ClearScreenRequest& e)
    {
        Clear(_clearColor);
        e.IsHandled = true;
    }

    void NVRHIRendererPlugin::OnFlushRequest(Tbx::FlushRendererRequest& e)
    {
        Flush();
        e.IsHandled = true;
    }
}