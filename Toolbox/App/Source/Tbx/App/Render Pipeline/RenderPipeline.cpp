#include "Tbx/App/PCH.h"
#include "Tbx/App/Render Pipeline/RenderPipeline.h"
#include "Tbx/App/Render Pipeline/RenderQueue.h"
#include "Tbx/App/Events/RenderEvents.h"
#include "Tbx/App/Events/WindowEvents.h"
#include "Tbx/App/Windowing/WindowManager.h"
#include <Tbx/Core/Events/EventDispatcher.h>
#include <memory>

namespace Tbx
{
    bool RenderPipeline::_vsyncEnabled = false;

    RenderQueue RenderPipeline::_renderQueue;

    UID RenderPipeline::_focusedWindowId = -1;
    UID RenderPipeline::_appUpdatedEventId = -1;
    UID RenderPipeline::_windowResizeEventId = -1;
    UID RenderPipeline::_windowFocusChangedEventId = -1;

    void RenderPipeline::Initialize()
    {
        _appUpdatedEventId = EventDispatcher::Subscribe<AppUpdatedEvent>(TBX_BIND_STATIC_CALLBACK(OnAppUpdated));
        _windowFocusChangedEventId = EventDispatcher::Subscribe<WindowFocusChangedEvent>(TBX_BIND_STATIC_CALLBACK(OnWindowFocusChanged));
        _windowResizeEventId = EventDispatcher::Subscribe<WindowResizedEvent>(TBX_BIND_STATIC_CALLBACK(OnWindowResize));
    }

    void RenderPipeline::Shutdown()
    {
        EventDispatcher::Unsubscribe<AppUpdatedEvent>(_appUpdatedEventId);
        EventDispatcher::Unsubscribe<WindowFocusChangedEvent>(_windowFocusChangedEventId);
        EventDispatcher::Unsubscribe<WindowResizedEvent>(_windowResizeEventId);

        Flush();
    }

    void RenderPipeline::SetVSyncEnabled(bool enabled)
    {
        _vsyncEnabled = enabled;

        SetVSyncRequestEvent request(enabled);
        EventDispatcher::Dispatch(request);
    }

    bool RenderPipeline::IsVSyncEnabled()
    {
        return _vsyncEnabled;
    }

    void RenderPipeline::Push(const RenderCommand& command, const std::any& data)
    {
        if (_renderQueue.IsEmpty())
        {
            auto renderBatch = RenderBatch();
            renderBatch.AddItem({ command, data });
            _renderQueue.Push(renderBatch);
        }
        else
        {
            auto& frameBatch = _renderQueue.Peek();
            frameBatch.AddItem({ command, data });
        }
    }

    void RenderPipeline::Clear()
    {
        ClearScreenRequestEvent request;
        EventDispatcher::Dispatch(request);
    }

    void RenderPipeline::Flush()
    {
        FlushRendererRequestEvent request;
        EventDispatcher::Dispatch(request);
        _renderQueue.Clear();
    }

    void RenderPipeline::ProcessNextBatch()
    {
        BeginRenderFrameRequestEvent beginFrameRequest;
        EventDispatcher::Dispatch(beginFrameRequest);

        if (!_renderQueue.IsEmpty())
        {
            const auto& batch = _renderQueue.Peek();
            RenderFrameRequestEvent request(batch);
            EventDispatcher::Dispatch(request);
            _renderQueue.Pop();
        }

        EndRenderFrameRequestEvent endFrameRequest;
        EventDispatcher::Dispatch(beginFrameRequest);
    }

    void RenderPipeline::OnAppUpdated(const AppUpdatedEvent&)
    {
        ProcessNextBatch();
    }

    void RenderPipeline::OnWindowFocusChanged(const WindowFocusChangedEvent& e)
    {
        if (!e.IsFocused()) return;

        auto request = SetRenderContextRequestEvent(WindowManager::GetWindow(e.GetWindowId()));
        EventDispatcher::Dispatch(request);
    }

    void RenderPipeline::OnWindowResize(const WindowResizedEvent& e)
    {
        std::weak_ptr<IWindow> windowThatWasResized = WindowManager::GetWindow(e.GetWindowId());

        // Draw the window while its resizing so there are no artifacts during the resize
        const bool& wasVSyncEnabled = RenderPipeline::IsVSyncEnabled();
        SetVSyncEnabled(true); // Enable vsync so the window doesn't flicker
        ProcessNextBatch();
        SetVSyncEnabled(wasVSyncEnabled); // Set vsync back to what it was

        // Log window resize
        const auto& newSize = windowThatWasResized.lock()->GetSize();
        const auto& name = windowThatWasResized.lock()->GetTitle();
        TBX_INFO("Window {0} resized to {1}x{2}", name, newSize.Width, newSize.Height);
    }
}
