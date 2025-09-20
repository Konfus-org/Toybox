#include "Tbx/PCH.h"
#include "Tbx/Graphics/Rendering.h"
#include "Tbx/Graphics/FrameBufferBuilder.h"
#include <algorithm>
#include <cstddef>
#include <iterator>

namespace Tbx
{
    Rendering::Rendering(Tbx::Ref<IRendererFactory> rendererFactory, Tbx::Ref<EventBus> eventBus)
        : _rendererFactory(rendererFactory)
        , _eventBus(eventBus)
    {
        TBX_ASSERT(_rendererFactory, "Rendering requires a valid renderer factory instance.");
        TBX_ASSERT(_eventBus, "Rendering requires a valid event bus instance.");

        _eventBus->Subscribe(this, &Rendering::OnWindowOpened);
        _eventBus->Subscribe(this, &Rendering::OnWindowClosed);
        _eventBus->Subscribe(this, &Rendering::OnAppSettingsChanged);
        _eventBus->Subscribe(this, &Rendering::OnStageOpened);
        _eventBus->Subscribe(this, &Rendering::OnStageClosed);
    }

    Rendering::~Rendering()
    {
        if (_eventBus)
        {
            _eventBus->Unsubscribe(this, &Rendering::OnWindowOpened);
            _eventBus->Unsubscribe(this, &Rendering::OnWindowClosed);
            _eventBus->Unsubscribe(this, &Rendering::OnAppSettingsChanged);
            _eventBus->Unsubscribe(this, &Rendering::OnStageOpened);
            _eventBus->Unsubscribe(this, &Rendering::OnStageClosed);
        }
    }

    void Rendering::Update()
    {
        if (_renderers.empty() || _openStages.empty())
        {
            return;
        }

        DrawFrame();
    }

    Tbx::Ref<IRenderer> Rendering::GetRenderer(const Tbx::Ref<IWindow>& window) const
    {
        if (!window)
        {
            return nullptr;
        }

        auto it = std::find(_windows.begin(), _windows.end(), window);
        if (it == _windows.end())
        {
            TBX_ASSERT(false, "No renderer found for window");
            return nullptr;
        }

        const auto index = static_cast<size_t>(std::distance(_windows.begin(), it));
        return index < _renderers.size() ? _renderers[index] : nullptr;
    }

    void Rendering::DrawFrame()
    {
        FrameBuffer uploadBuffer = {};
        FrameBuffer renderBuffer = {};

        for (const auto& stage : _openStages)
        {
            if (!stage)
            {
                continue;
            }

            const auto spaceRoot = stage->GetRoot();
            if (!spaceRoot)
            {
                continue;
            }

            FrameBufferBuilder builder;

            if (_firstFrame)
            {
                const auto stageUploadBuffer = builder.BuildUploadBuffer(spaceRoot);
                for (const auto& command : stageUploadBuffer)
                {
                    uploadBuffer.Add(command);
                }
            }

            const auto stageRenderBuffer = builder.BuildRenderBuffer(spaceRoot);
            for (const auto& command : stageRenderBuffer)
            {
                renderBuffer.Add(command);
            }
        }

        if (_firstFrame)
        {
            for (const auto& renderer : _renderers)
            {
                if (!renderer)
                {
                    continue;
                }

                renderer->Flush();
                renderer->Process(uploadBuffer);
            }

            _firstFrame = false;
        }

        for (size_t rendererIndex = 0; rendererIndex < _renderers.size(); ++rendererIndex)
        {
            const auto& renderer = _renderers[rendererIndex];
            const auto& rendererWindow = _windows[rendererIndex];
            if (!renderer || !rendererWindow)
            {
                continue;
            }

            renderer->SetViewport({ {0, 0}, rendererWindow->GetSize() });
            renderer->Clear(_clearColor);
            renderer->Process(renderBuffer);
        }

        if (_eventBus)
        {
            _eventBus->Send(RenderedFrameEvent());
        }

        for (const auto& window : _windows)
        {
            if (window)
            {
                window->SwapBuffers();
            }
        }
    }

    void Rendering::ResetFirstFrame()
    {
        _firstFrame = true;
    }

    void Rendering::AddStage(const Tbx::Ref<Stage>& stage)
    {
        if (!stage)
        {
            return;
        }

        const auto it = std::find(_openStages.begin(), _openStages.end(), stage);
        if (it != _openStages.end())
        {
            return;
        }

        _openStages.push_back(stage);
        ResetFirstFrame();
    }

    void Rendering::RemoveStage(const Tbx::Ref<Stage>& stage)
    {
        if (!stage)
        {
            return;
        }

        auto it = std::find(_openStages.begin(), _openStages.end(), stage);
        if (it != _openStages.end())
        {
            _openStages.erase(it);
            ResetFirstFrame();
        }
    }

    void Rendering::OnWindowOpened(const WindowOpenedEvent& e)
    {
        auto newWindow = e.GetWindow();
        if (!newWindow)
        {
            return;
        }

        TBX_ASSERT(_rendererFactory, "Render factory plugin was unloaded! Cannot create new renderer");
        if (!_rendererFactory)
        {
            return;
        }

        auto newRenderer = _rendererFactory->Create(newWindow);
        _windows.push_back(newWindow);
        _renderers.push_back(newRenderer);
        ResetFirstFrame();
    }

    void Rendering::OnWindowClosed(const WindowClosedEvent& e)
    {
        auto window = e.GetWindow();
        auto it = std::find(_windows.begin(), _windows.end(), window);
        if (it == _windows.end())
        {
            return;
        }

        const auto index = static_cast<size_t>(std::distance(_windows.begin(), it));
        _windows.erase(_windows.begin() + static_cast<std::ptrdiff_t>(index));
        if (index < _renderers.size())
        {
            _renderers.erase(_renderers.begin() + static_cast<std::ptrdiff_t>(index));
        }

        ResetFirstFrame();
    }

    void Rendering::OnAppSettingsChanged(const AppSettingsChangedEvent& e)
    {
        _clearColor = e.GetNewSettings().ClearColor;
    }

    void Rendering::OnStageOpened(const StageOpenedEvent& e)
    {
        AddStage(e.GetStage());
    }

    void Rendering::OnStageClosed(const StageClosedEvent& e)
    {
        RemoveStage(e.GetStage());
    }
}

