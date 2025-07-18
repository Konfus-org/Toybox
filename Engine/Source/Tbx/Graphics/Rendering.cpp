#include "Tbx/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/Graphics/Rendering.h"
#include "Tbx/Graphics/IRenderer.h"
#include "Tbx/Graphics/Mesh.h"
#include "Tbx/Graphics/RenderProcessor.h"
#include "Tbx/Events/EventCoordinator.h"
#include "Tbx/Events/RenderEvents.h"
#include "Tbx/TBS/World.h"
#include "Tbx/Plugin API/PluginServer.h"
#include <iostream>

namespace Tbx
{
    std::shared_ptr<IRendererFactoryPlugin> Rendering::_renderFactory = nullptr;
    std::map<UID, std::shared_ptr<IRenderer>> Rendering::_renderers = {};
    UID Rendering::_onWindowCreatedEventId = -1;
    UID Rendering::_onWindowClosedEventId = -1;

    void Rendering::Initialize()
    {
        _onWindowCreatedEventId = EventCoordinator::Subscribe<WindowOpenedEvent>(TBX_BIND_STATIC_FN(Rendering::OnWindowCreated));
        _onWindowClosedEventId = EventCoordinator::Subscribe<WindowClosedEvent>(TBX_BIND_STATIC_FN(Rendering::OnWindowClosed));

        _renderFactory = PluginServer::GetPlugin<IRendererFactoryPlugin>();
    }

    void Rendering::Shutdown()
    {
        EventCoordinator::Unsubscribe<WindowOpenedEvent>(_onWindowCreatedEventId);
        EventCoordinator::Unsubscribe<WindowClosedEvent>(_onWindowClosedEventId);
    }

    void Rendering::DrawFrame()
    {
        // TODO: This is testing code!!! Need actual implementation
        FrameBuffer buffer;
        auto triangleMesh = Primitives::Triangle;
        buffer.Add({ DrawCommandType::DrawMesh, triangleMesh });

        auto windows = App::GetInstance()->GetWindows();
        for (const auto& window : windows)
        {
            auto winId = window->GetId();
            if (!_renderers.contains(winId)) continue;

            _renderers[winId]->Flush();
            _renderers[winId]->Draw(buffer);
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
        auto newWinId = e.GetWindowId();
        auto newWindow = App::GetInstance()->GetWindow(newWinId);
        _renderers[newWinId] = _renderFactory->Create(newWindow);
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
    //    for (const auto& playspaceId : r.GetBoxesToOpen())
    //    {
    //        playspaceToOpen.push_back(World::GetPlayspace(playspaceId));
    //    }

    //    auto buffer = RenderProcessor::PreProcess(playspaceToOpen);
    //    // TODO: implement
    //    //TBX_ASSERT(request.IsHandled, "Render frame request was not handled. Is a renderer created and listening?");

    //    r.IsHandled = true;
    //}
}