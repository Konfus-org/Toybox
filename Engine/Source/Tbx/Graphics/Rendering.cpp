#include "Tbx/PCH.h"
#include "Tbx/TBS/World.h"
#include "Tbx/Graphics/Rendering.h"
#include "Tbx/Graphics/IRenderer.h"
#include "Tbx/Graphics/FrameBufferBuilder.h"
#include "Tbx/Events/EventCoordinator.h"
#include "Tbx/Events/RenderEvents.h"
#include "Tbx/PluginAPI/PluginServer.h"

namespace Tbx
{
    Rendering::Rendering()
    {
        EventCoordinator::Subscribe(this, &Rendering::OnWindowOpened);
        EventCoordinator::Subscribe(this, &Rendering::OnWindowClosed);
        EventCoordinator::Subscribe(this, &Rendering::OnWindowResized);
        EventCoordinator::Subscribe(this, &Rendering::OnGraphicsSettingsChanged);

        _renderFactory = PluginServer::Get<IRendererFactoryPlugin>();
    }

    Rendering::~Rendering()
    {
        EventCoordinator::Unsubscribe(this, &Rendering::OnWindowOpened);
        EventCoordinator::Unsubscribe(this, &Rendering::OnWindowClosed);
        EventCoordinator::Unsubscribe(this, &Rendering::OnWindowResized);
        EventCoordinator::Unsubscribe(this, &Rendering::OnGraphicsSettingsChanged);
    }

    void Rendering::DrawFrame(const std::shared_ptr<World>& world)
    {
        if (!world)
        {
            return;
        }

        // Gather all boxes from the current world
        const auto worldRoot = world->GetRoot();

        if (_firstFrame)
        {
            // Pre-process the opened box using the frame buffer builder
            FrameBufferBuilder builder;
            const auto buffer = builder.BuildUploadBuffer(worldRoot);

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
        const auto buffer = builder.BuildRenderBuffer(worldRoot);

        // Send buffer to renderers for each window
        for (const auto& renderer : _renderers)
        {
            renderer->Clear(_graphicsSettings.ClearColor);
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

    std::shared_ptr<IRenderer> Rendering::GetRenderer(const std::shared_ptr<IWindow>& window)
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

    void Rendering::OnWindowOpened(const WindowOpenedEvent& e)
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

    void Rendering::OnWindowClosed(const WindowClosedEvent& e)
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

    void Rendering::OnWindowResized(const WindowResizedEvent& e)
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

    void Rendering::OnGraphicsSettingsChanged(const AppGraphicsSettingsChangedEvent& e)
    {
        _graphicsSettings = e.GetNewSettings();
    }
}

