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
    Uid Rendering::_windowCreatedEventId = Invalid::Uid;
    Uid Rendering::_windowClosedEventId = Invalid::Uid;
    Uid Rendering::_windowResizedEventId = Invalid::Uid;
    Uid Rendering::_boxOpenedEventId = Invalid::Uid;

    void Rendering::Initialize()
    {
        _windowCreatedEventId = EventCoordinator::Subscribe<WindowOpenedEvent>(TBX_BIND_STATIC_FN(Rendering::OnWindowOpened));
        _windowClosedEventId = EventCoordinator::Subscribe<WindowClosedEvent>(TBX_BIND_STATIC_FN(Rendering::OnWindowClosed));
        _windowResizedEventId = EventCoordinator::Subscribe<WindowResizedEvent>(TBX_BIND_STATIC_FN(Rendering::OnWindowResized));
        _boxOpenedEventId = EventCoordinator::Subscribe<OpenedBoxEvent>(TBX_BIND_STATIC_FN(Rendering::OnBoxOpened));

        _renderFactory = PluginServer::GetPlugin<IRendererFactoryPlugin>();
    }

    void Rendering::Shutdown()
    {
        EventCoordinator::Unsubscribe<WindowOpenedEvent>(_windowCreatedEventId);
        EventCoordinator::Unsubscribe<WindowClosedEvent>(_windowClosedEventId);
        EventCoordinator::Unsubscribe<WindowClosedEvent>(_windowResizedEventId);
        EventCoordinator::Unsubscribe<WindowClosedEvent>(_boxOpenedEventId);

        _renderFactory = nullptr;
    }

    void Rendering::DrawFrame()
    {
        // Gather all boxes from the current world
        const auto boxes = World::GetBoxes();

        // Process the world using the render processor
        const auto buffer = RenderProcessor::Process(boxes);

        // Send buffer to renderers for each window
        auto windows = App::GetInstance()->GetWindows();
        for (const auto& window : windows)
        {
            auto winId = window->GetId();
            if (!_renderers.contains(winId)) continue;

            auto renderer = _renderers[winId];
            renderer->Clear(App::GetInstance()->GetGraphicsSettings().ClearColor);
            renderer->Process(buffer);
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
            TBX_ASSERT(_renderers[windowId] != nullptr, "Renderer should not be null");
            _renderers.erase(windowId);
        }
    }

    void Rendering::OnWindowResized(const WindowResizedEvent& e)
    {
        auto windowId = e.GetWindowId();
        if (_renderers.contains(windowId))
        {
            const auto& newSize = e.GetNewSize();
            auto renderer = _renderers[windowId];
            TBX_ASSERT(renderer != nullptr, "Renderer should not be null");
            renderer->SetViewport({{0, 0}, newSize});
        }
    }

    void Rendering::OnBoxOpened(const OpenedBoxEvent& e)
    {
        // Gather all boxes from the current world
        const auto box = World::GetBox(e.GetOpenedBox());

        // Pre-process the opened box using the render processor
        const auto buffer = RenderProcessor::PreProcess({box});

        // Send buffer to renderers for each window
        const auto& windows = App::GetInstance()->GetWindows();
        for (const auto& window : windows)
        {
            auto winId = window->GetId();
            if (!_renderers.contains(winId)) continue;

            _renderers[winId]->Flush();
            _renderers[winId]->Process(buffer);
        }
    }
}