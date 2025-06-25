#include "Tbx/PCH.h"
#include "Tbx/Graphics/Rendering.h"
#include "Tbx/Graphics/IRenderer.h"
#include "Tbx/Graphics/RenderProcessor.h"
#include "Tbx/Windowing/WindowManager.h"
#include "Tbx/Events/EventCoordinator.h"
#include "Tbx/Events/RenderEvents.h"
#include "Tbx/TBS/World.h"
#include <iostream>

namespace Tbx
{
    std::map<UID, std::shared_ptr<IRenderer>> Rendering::_renderers = {};
    UID Rendering::_onWindowCreatedEventId = -1;
    UID Rendering::_onWindowClosedEventId = -1;

    void Rendering::Initialize()
    {
        _onWindowCreatedEventId = EventCoordinator::Subscribe<WindowOpenedEvent>(TBX_BIND_STATIC_FN(Rendering::OnWindowCreated));
        _onWindowClosedEventId = EventCoordinator::Subscribe<WindowClosedEvent>(TBX_BIND_STATIC_FN(Rendering::OnWindowClosed));
    }

    void Rendering::Shutdown()
    {
        EventCoordinator::Unsubscribe<WindowOpenedEvent>(_onWindowCreatedEventId);
        EventCoordinator::Unsubscribe<WindowClosedEvent>(_onWindowClosedEventId);
    }

    void Rendering::DrawFrame()
    {
        FrameBuffer buffer;
        auto clearColor = Tbx::Color(1, 0.1f, 0.1f, 1);
        buffer.Add({ DrawCommandType::Clear, clearColor });

        for (const auto& window : WindowManager::GetAllWindows())
        {
            _renderers[window->GetId()]->Draw(buffer);
        }

        for (const auto& window : WindowManager::GetAllWindows())
        {
            _renderers[window->GetId()]->Flush();
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

    //void Rendering::OnOpenPlayspacesRequest(OpenPlayspacesRequest& r)
    //{
    //    auto playspaceToOpen = std::vector<std::shared_ptr<Playspace>>();
    //    for (const auto& playspaceId : r.GetPlaySpacesToOpen())
    //    {
    //        playspaceToOpen.push_back(World::GetPlayspace(playspaceId));
    //    }

    //    auto buffer = RenderProcessor::PreProcess(playspaceToOpen);
    //    // TODO: implement
    //    //TBX_ASSERT(request.IsHandled, "Render frame request was not handled. Is a renderer created and listening?");

    //    r.IsHandled = true;
    //}
}