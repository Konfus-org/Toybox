#include "Tbx/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/Graphics/Rendering.h"
#include "Tbx/Graphics/IRenderer.h"
#include "Tbx/Graphics/FrameBufferBuilder.h"
#include "Tbx/Events/EventCoordinator.h"
#include "Tbx/Events/RenderEvents.h"
#include "Tbx/TBS/World.h"
#include "Tbx/PluginAPI/PluginServer.h"

namespace Tbx
{
    std::weak_ptr<IRendererFactoryPlugin> Rendering::_renderFactory = {};
    std::map<Uid, std::shared_ptr<IRenderer>> Rendering::_renderers = {};
    void Rendering::Initialize()
    {
        EventCoordinator::Subscribe<WindowOpenedEvent>(&Rendering::OnWindowOpened);
        EventCoordinator::Subscribe<WindowClosedEvent>(&Rendering::OnWindowClosed);
        EventCoordinator::Subscribe<WindowResizedEvent>(&Rendering::OnWindowResized);
        EventCoordinator::Subscribe<OpenedBoxEvent>(&Rendering::OnBoxOpened);

        _renderFactory = PluginServer::Get<IRendererFactoryPlugin>();
    }

    void Rendering::Shutdown()
    {
        EventCoordinator::Unsubscribe<WindowOpenedEvent>(&Rendering::OnWindowOpened);
        EventCoordinator::Unsubscribe<WindowClosedEvent>(&Rendering::OnWindowClosed);
        EventCoordinator::Unsubscribe<WindowResizedEvent>(&Rendering::OnWindowResized);
        EventCoordinator::Unsubscribe<OpenedBoxEvent>(&Rendering::OnBoxOpened);
    }

    void Rendering::DrawFrame()
    {
        // Gather all boxes from the current world
        const auto boxes = World::GetBoxes();

        // Build a frame buffer of render commands for the world
        FrameBufferBuilder builder;
        const auto buffer = builder.BuildRenderBuffer(boxes);

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

        // Send our frame rendered event so anything can hook into our rendering and do post work...
        RenderedFrameEvent evt;
        EventCoordinator::Send(evt);

        // Swap the buffers for each window after a frame is rendered
        for (const auto& window : windows)
        {
            window->SwapBuffers();
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
        if (_renderFactory.expired() || !_renderFactory.lock())
        {
            TBX_ASSERT(false, "Render factory plugin was unloaded! Cannot create new renderer");
            return;
        }

        auto newRenderer = _renderFactory.lock()->Create(newWindow);
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

        // Update viewports
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
        // Gather all boxes from the current box
        const auto box = World::GetBox(e.GetOpenedBox());

        // Pre-process the opened box using the frame buffer builder
        FrameBufferBuilder builder;
        const auto buffer = builder.BuildUploadBuffer({box});

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