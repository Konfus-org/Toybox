#include "Tbx/Systems/PCH.h"
#include "Tbx/Systems/Rendering/Rendering.h"
#include "Tbx/Systems/Rendering/IRenderer.h"
#include "Tbx/Systems/Rendering/RenderEvents.h"
#include "Tbx/Systems/Windowing/WindowManager.h"
#include "Tbx/Systems/Events/EventCoordinator.h"
#include <iostream>

namespace Tbx
{
    RenderPipeline Rendering::_pipeline = {};
    std::map<UID, std::shared_ptr<IRenderer>> Rendering::_renderers = {};
    UID Rendering::_onWindowCreatedEventId = -1;

    void Rendering::Initialize()
    {
        EventCoordinator::Subscribe<WindowOpenedEvent>(TBX_BIND_STATIC_FN(Rendering::OnWindowCreated));
        EventCoordinator::Subscribe<WindowClosedEvent>(TBX_BIND_STATIC_FN(Rendering::OnWindowClosed));
    }

    void Rendering::Shutdown()
    {
        EventCoordinator::Unsubscribe<WindowOpenedEvent>(_onWindowCreatedEventId);
        EventCoordinator::Unsubscribe<WindowClosedEvent>(_onWindowCreatedEventId);
    }

    void Rendering::DrawFrame()
    {
        // TODO: Process world via pipeline...
    }
    
    std::shared_ptr<IRenderer> Rendering::GetRenderer(UID window)
    {
        if (!_renderers.contains(window))
        {
            TBX_ASSERT(false, "No renderer found for window ID: {0}", window.Value);
            return nullptr;
        }

        return _renderers[window];
    }
    
    void Rendering::OnWindowCreated(const WindowOpenedEvent& event)
    {
        auto newWindow = WindowManager::GetWindow(event.GetWindowId());
        auto createRendererRequest = CreateRendererRequest(newWindow);

        EventCoordinator::Send(createRendererRequest);
        TBX_ASSERT(createRendererRequest.IsHandled, "Failed to create renderer for window ID: {0}", event.GetWindowId().Value);

        _renderers[event.GetWindowId()] = createRendererRequest.GetResult();
    }

    void Rendering::OnWindowClosed(const WindowClosedEvent& event)
    {
        if (_renderers.contains(event.GetWindowId()))
        {
            _renderers.erase(event.GetWindowId());
        }
    }
}