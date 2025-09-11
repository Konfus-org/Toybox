#include "Tbx/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/Layers/RenderingLayer.h"
#include "Tbx/Graphics/FrameBufferBuilder.h"
#include "Tbx/Events/EventCoordinator.h"
#include "Tbx/Events/RenderEvents.h"
#include "Tbx/PluginAPI/PluginServer.h"

namespace Tbx
{
    RenderingLayer::RenderingLayer(const std::weak_ptr<App> app) : Layer("Rendering", app)
    {
        EventCoordinator::Subscribe(this, &RenderingLayer::OnWindowOpened);
        EventCoordinator::Subscribe(this, &RenderingLayer::OnWindowClosed);
        EventCoordinator::Subscribe(this, &RenderingLayer::OnWindowResized);
        EventCoordinator::Subscribe(this, &RenderingLayer::OnAppSettingsChanged);

        _renderFactory = PluginServer::Get<IRendererFactoryPlugin>();
    }

    RenderingLayer::~RenderingLayer()
    {
        EventCoordinator::Unsubscribe(this, &RenderingLayer::OnWindowOpened);
        EventCoordinator::Unsubscribe(this, &RenderingLayer::OnWindowClosed);
        EventCoordinator::Unsubscribe(this, &RenderingLayer::OnWindowResized);
        EventCoordinator::Unsubscribe(this, &RenderingLayer::OnAppSettingsChanged);
    }

    void RenderingLayer::OnUpdate()
    {
        DrawFrame();
    }

    void RenderingLayer::DrawFrame()
    {
        auto world = GetApp()->GetWorld();
        if (!world)
        {
            return;
        }

        // TODO: add support for layers!

        // Gather all boxes from the current world
        const std::shared_ptr<Toy> spaceRoot = world->GetRoot();

        if (_firstFrame) // TODO: We need to do this any time we add a new renderer or toy...
        {
            // Pre-process the opened box using the frame buffer builder
            FrameBufferBuilder builder;
            const auto buffer = builder.BuildUploadBuffer(spaceRoot);

            // Send buffer to renderers for each window
            for (const auto& renderer : _renderers)
            {
                renderer->Flush();
                renderer->Process(buffer);
            }

            // Flip first frame flag to off
            _firstFrame = false;
        }

        // Build a frame buffer of render commands for the world
        FrameBufferBuilder builder;
        const auto buffer = builder.BuildRenderBuffer(spaceRoot);

        // Send buffer to renderers for each window
        for (const auto& renderer : _renderers)
        {
            renderer->Clear(_clearColor);
            renderer->Process(buffer);
        }

        // Send our frame rendered event so anything can hook into our rendering and do post work...
        RenderedFrameEvent evt;
        EventCoordinator::Send(evt);

        // Swap the buffers for each window after a frame is rendered
        for (const auto& window : _windows)
        {
            window->SwapBuffers();
        }
    }

    std::shared_ptr<IRenderer> RenderingLayer::GetRenderer(const std::shared_ptr<IWindow>& window)
    {
        auto it = std::find(_windows.begin(), _windows.end(), window);
        if (it == _windows.end())
        {
            TBX_ASSERT(false, "No renderer found for window");
            return nullptr;
        }
        auto index = static_cast<size_t>(std::distance(_windows.begin(), it));
        return _renderers[index];
    }

    void RenderingLayer::OnWindowOpened(const WindowOpenedEvent& e)
    {
        auto newWindow = e.GetWindow();
        if (_renderFactory.expired() || !_renderFactory.lock())
        {
            TBX_ASSERT(false, "Render factory plugin was unloaded! Cannot create new renderer");
            return;
        }

        auto newRenderer = _renderFactory.lock()->Create(newWindow);
        _windows.push_back(newWindow);
        _renderers.push_back(newRenderer);
    }

    void RenderingLayer::OnWindowClosed(const WindowClosedEvent& e)
    {
        auto window = e.GetWindow();
        auto it = std::find(_windows.begin(), _windows.end(), window);
        if (it != _windows.end())
        {
            auto index = static_cast<size_t>(std::distance(_windows.begin(), it));
            _windows.erase(_windows.begin() + index);
            if (index < _renderers.size())
            {
                _renderers.erase(_renderers.begin() + index);
            }
        }
    }

    void RenderingLayer::OnWindowResized(const WindowResizedEvent& e)
    {
        auto window = e.GetWindow();
        auto it = std::find(_windows.begin(), _windows.end(), window);
        if (it != _windows.end())
        {
            auto index = static_cast<size_t>(std::distance(_windows.begin(), it));
            if (index < _renderers.size())
            {
                const auto& newSize = e.GetNewSize();
                auto renderer = _renderers[index];
                TBX_ASSERT(renderer != nullptr, "Renderer should not be null");
                renderer->SetViewport({ {0, 0}, newSize });
            }
        }
    }

    void RenderingLayer::OnAppSettingsChanged(const AppSettingsChangedEvent& e)
    {
        _clearColor = e.GetNewSettings().ClearColor;
    }
}

