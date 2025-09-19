#include "Tbx/PCH.h"
#include "Tbx/Layers/RenderingLayer.h"
#include "Tbx/ECS/ThreeDSpace.h"
#include "Tbx/Graphics/FrameBufferBuilder.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/RenderEvents.h"

namespace Tbx
{
    RenderingLayer::RenderingLayer(
        Tbx::Ref<IRendererFactory> renderFactory,
        Tbx::Ref<EventBus> eventBus) : Layer("Rendering")
    {
        _renderFactory = renderFactory;
        _eventBus = eventBus;

        _eventBus->Subscribe(this, &RenderingLayer::OnWindowOpened);
        _eventBus->Subscribe(this, &RenderingLayer::OnWindowClosed);
        _eventBus->Subscribe(this, &RenderingLayer::OnAppSettingsChanged);
    }

    void RenderingLayer::OnUpdate()
    {
        DrawFrame();
    }

    void RenderingLayer::DrawFrame()
    {
        // TODO: add support for world layers!

        // Gather all boxes from the current world
        const Tbx::Ref<Toy> spaceRoot = _worldSpace->GetRoot();

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
        int rendererIndex = 0;
        for (const auto& renderer : _renderers)
        {
            auto rendererWindow = _windows[rendererIndex];
            renderer->SetViewport({ {0, 0}, rendererWindow->GetSize() });
            renderer->Clear(_clearColor);
            renderer->Process(buffer);
            rendererIndex++;
        }

        // Send our frame rendered event so anything can hook into our rendering and do post work...
        _eventBus->Send(RenderedFrameEvent());

        // Swap the buffers for each window after a frame is rendered
        for (const auto& window : _windows)
        {
            window->SwapBuffers();
        }
    }

    Tbx::Ref<IRenderer> RenderingLayer::GetRenderer(const Tbx::Ref<IWindow>& window)
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
        TBX_ASSERT(_renderFactory, "Render factory plugin was unloaded! Cannot create new renderer");

        auto newRenderer = _renderFactory->Create(newWindow);
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

    void RenderingLayer::OnAppSettingsChanged(const AppSettingsChangedEvent& e)
    {
        _clearColor = e.GetNewSettings().ClearColor;
    }
}

