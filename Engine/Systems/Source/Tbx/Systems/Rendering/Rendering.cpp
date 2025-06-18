#include "Tbx/Systems/PCH.h"
#include "Tbx/Systems/Rendering/Rendering.h"
#include "Tbx/Systems/Rendering/IRenderer.h"
#include "Tbx/Systems/Rendering/RenderEvents.h"
#include "Tbx/Systems/Rendering/RenderProcessor.h"
#include "Tbx/Systems/Windowing/WindowManager.h"
#include "Tbx/Systems/Events/EventCoordinator.h"
#include "Tbx/Systems/TBS/World.h"
#include <iostream>

namespace Tbx
{
    std::map<UID, std::shared_ptr<IRenderer>> Rendering::_renderers = {};
    UID Rendering::_onWindowCreatedEventId = -1;

    void Rendering::Initialize()
    {
        EventCoordinator::Subscribe<WindowOpenedEvent>(TBX_BIND_STATIC_FN(Rendering::OnWindowCreated));
        EventCoordinator::Subscribe<WindowClosedEvent>(TBX_BIND_STATIC_FN(Rendering::OnWindowClosed));
        EventCoordinator::Subscribe<OpenPlayspacesRequest>(TBX_BIND_STATIC_FN(Rendering::OnOpenPlayspacesRequest));
    }

    void Rendering::Shutdown()
    {
        EventCoordinator::Unsubscribe<WindowOpenedEvent>(_onWindowCreatedEventId);
        EventCoordinator::Unsubscribe<WindowClosedEvent>(_onWindowCreatedEventId);
    }

    void Rendering::DrawFrame()
    {
        for (const auto& playspace : World::GetPlayspaces())
        {
            auto buffer = RenderProcessor::Process(playspace);

            auto request = RenderFrameRequest(buffer);
            EventCoordinator::Send(request);

            TBX_ASSERT(request.IsHandled, "Render frame request was not handled. Is a renderer created and listening?");
        }
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
    
    void Rendering::OnWindowCreated(const WindowOpenedEvent& e)
    {
        auto newWindow = WindowManager::GetWindow(e.GetWindowId());
        auto createRendererRequest = CreateRendererRequest(newWindow);

        EventCoordinator::Send(createRendererRequest);
        TBX_ASSERT(createRendererRequest.IsHandled, "Failed to create renderer for window ID: {0}", e.GetWindowId().Value);

        _renderers[e.GetWindowId()] = createRendererRequest.GetResult();
    }

    void Rendering::OnWindowClosed(const WindowClosedEvent& e)
    {
        if (_renderers.contains(e.GetWindowId()))
        {
            _renderers.erase(e.GetWindowId());
        }
    }

    void Rendering::OnOpenPlayspacesRequest(OpenPlayspacesRequest& r)
    {
        for (const auto& playspaceId : r.GetPlaySpacesToOpen())
        {
            auto playspace = World::GetPlayspace(playspaceId);

            auto buffer = RenderProcessor::PreProcess(playspace);
            auto request = RenderFrameRequest(buffer);
            EventCoordinator::Send(request);

            TBX_ASSERT(request.IsHandled, "Render frame request was not handled. Is a renderer created and listening?");
        }

        r.IsHandled = true;
    }
}