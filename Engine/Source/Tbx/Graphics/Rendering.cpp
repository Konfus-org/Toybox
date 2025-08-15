#include "Tbx/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/Graphics/Rendering.h"
#include "Tbx/Graphics/IRenderer.h"
#include "Tbx/Graphics/RenderProcessor.h"
#include "Tbx/Events/EventCoordinator.h"
#include "Tbx/Events/RenderEvents.h"
#include "Tbx/TBS/World.h"
#include "Tbx/PluginAPI/PluginServer.h"

namespace Tbx
{
    std::shared_ptr<IRendererFactoryPlugin> Rendering::_renderFactory = nullptr;
    std::map<Uid, std::shared_ptr<IRenderer>> Rendering::_renderers = {};
    Uid Rendering::_onWindowCreatedEventId = Invalid::Uid;
    Uid Rendering::_onWindowClosedEventId = Invalid::Uid;

    void Rendering::Initialize()
    {
        _onWindowCreatedEventId = EventCoordinator::Subscribe<WindowOpenedEvent>(TBX_BIND_STATIC_FN(Rendering::OnWindowOpened));
        _onWindowClosedEventId = EventCoordinator::Subscribe<WindowClosedEvent>(TBX_BIND_STATIC_FN(Rendering::OnWindowClosed));

        _renderFactory = PluginServer::GetPlugin<IRendererFactoryPlugin>();
    }

    void Rendering::Shutdown()
    {
        EventCoordinator::Unsubscribe<WindowOpenedEvent>(_onWindowCreatedEventId);
        EventCoordinator::Unsubscribe<WindowClosedEvent>(_onWindowClosedEventId);

        _renderFactory = nullptr;
    }

    void Rendering::DrawFrame()
    {
        // Gather all boxes from the current world
        const auto boxes = World::GetBoxes();

        // Preprocess and process the world using the render processor
        FrameBuffer buffer;
        const auto preBuffer = RenderProcessor::PreProcess(boxes);
        for (const auto& cmd : preBuffer.GetCommands())
        {
            buffer.Add(cmd);
        }
        const auto processed = RenderProcessor::Process(boxes);
        for (const auto& cmd : processed.GetCommands())
        {
            buffer.Add(cmd);
        }

        auto windows = App::GetInstance()->GetWindows();
        for (const auto& window : windows)
        {
            auto winId = window->GetId();
            if (!_renderers.contains(winId)) continue;

            _renderers[winId]->Flush();
            _renderers[winId]->Draw(buffer);
        }
    }

    std::shared_ptr<IRenderer> Rendering::GetRenderer(Uid window)
    {
        if (!_renderers.contains(window))
        {
            TBX_ASSERT(false, "No renderer found for window ID: {0}", window.Value);
            return nullptr;
        }

        return _renderers[window];
    }

    void Rendering::OnWindowOpened(const WindowOpenedEvent& e)
    {
        auto newWinId = e.GetWindowId();
        auto newWindow = App::GetInstance()->GetWindow(newWinId);

        auto newRenderer = _renderFactory->Create(newWindow);

        _renderers[newWinId] = newRenderer;
    }

    void Rendering::OnWindowClosed(const WindowClosedEvent& e)
    {
        auto windowId = e.GetWindowId();
        if (_renderers.contains(windowId))
        {
            // Log or assert the state before erasing
            TBX_ASSERT(_renderers[windowId] != nullptr, "Renderer should not be null");
            _renderers.erase(windowId);
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