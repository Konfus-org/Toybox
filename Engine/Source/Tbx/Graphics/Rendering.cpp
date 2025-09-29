#include "Tbx/PCH.h"
#include "Tbx/Graphics/Rendering.h"
#include "Tbx/Events/RenderEvents.h"
#include "Tbx/Graphics/RenderCommands.h"
#include "Tbx/Graphics/Viewport.h"
#include <algorithm>
#include <iterator>

namespace Tbx
{
    Rendering::Rendering(Ref<IRendererFactory> rendererFactory, Ref<EventBus> eventBus)
        : _rendererFactory(rendererFactory)
        , _eventBus(eventBus)
        , _eventListener(eventBus)
    {
        TBX_ASSERT(_rendererFactory, "Rendering: requires a valid renderer factory instance.");
        TBX_ASSERT(_eventBus, "Rendering: requires a valid event bus instance.");

        _eventListener.Listen<WindowOpenedEvent>(this, &Rendering::OnWindowOpened);
        _eventListener.Listen<WindowClosedEvent>(this, &Rendering::OnWindowClosed);
        _eventListener.Listen<WindowResizedEvent>(this, &Rendering::OnWindowResized);
        _eventListener.Listen<AppSettingsChangedEvent>(this, &Rendering::OnAppSettingsChanged);
        _eventListener.Listen<StageOpenedEvent>(this, &Rendering::OnStageOpened);
        _eventListener.Listen<StageClosedEvent>(this, &Rendering::OnStageClosed);
    }

    Rendering::~Rendering() = default;

    void Rendering::Update()
    {
        if (_renderers.empty() || _openStages.empty())
        {
            return;
        }

        DrawFrame();
    }

    Ref<IRenderer> Rendering::GetRenderer(const Ref<Window>& window) const
    {
        if (!window)
        {
            return nullptr;
        }

        auto it = std::find(_windows.begin(), _windows.end(), window);
        if (it == _windows.end())
        {
            TBX_ASSERT(false, "Rendering: No renderer found for window");
            return nullptr;
        }

        const auto index = static_cast<size_t>(std::distance(_windows.begin(), it));
        return index < _renderers.size() ? _renderers[index] : nullptr;
    }

    void Rendering::DrawFrame()
    {
        ProcessPendingUploads();
        ProcessOpenStages();
    }

    void Rendering::QueueStageUpload(const Ref<Stage>& stage)
    {
        if (!stage)
        {
            return;
        }

        const auto it = std::find(_pendingUploadStages.begin(), _pendingUploadStages.end(), stage);
        if (it == _pendingUploadStages.end())
        {
            _pendingUploadStages.push_back(stage);
        }
    }

    void Rendering::ProcessPendingUploads()
    {
        if (_pendingUploadStages.empty())
        {
            return;
        }

        RenderCommandBufferBuilder builder = {};
        RenderCommandBuffer uploadBuffer = {};
        for (const auto& stage : _pendingUploadStages)
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

            const auto stageUploadBuffer = builder.BuildUploadBuffer(spaceRoot);
            for (const auto& command : stageUploadBuffer.Commands)
            {
                uploadBuffer.Commands.push_back(command);
            }
        }

        if (!uploadBuffer.Commands.empty())
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
        }

        _pendingUploadStages.clear();
    }

    void Rendering::ProcessOpenStages()
    {
        RenderCommandBufferBuilder builder = {};
        RenderCommandBuffer renderBuffer = {};
        renderBuffer.Commands.emplace_back(RenderCommandType::Clear, _clearColor);

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

            const auto stageRenderBuffer = builder.BuildRenderBuffer(spaceRoot);
            for (const auto& command : stageRenderBuffer.Commands)
            {
                renderBuffer.Commands.push_back(command);
            }
        }

        for (size_t rendererIndex = 0; rendererIndex < _renderers.size(); ++rendererIndex)
        {
            const auto& renderer = _renderers[rendererIndex];
            const auto& rendererWindow = _windows[rendererIndex];
            if (!renderer || !rendererWindow)
            {
                continue;
            }

            renderer->Process(renderBuffer);
        }

        if (_eventBus)
        {
            _eventBus->Send(RenderedFrameEvent());
        }
    }

    void Rendering::AddStage(const Ref<Stage>& stage)
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
        QueueStageUpload(stage);
    }

    void Rendering::RemoveStage(const Ref<Stage>& stage)
    {
        if (!stage)
        {
            return;
        }

        auto it = std::find(_openStages.begin(), _openStages.end(), stage);
        if (it != _openStages.end())
        {
            _openStages.erase(it);
        }

        auto pending = std::find(_pendingUploadStages.begin(), _pendingUploadStages.end(), stage);
        if (pending != _pendingUploadStages.end())
        {
            _pendingUploadStages.erase(pending);
        }
    }

    void Rendering::OnWindowOpened(const WindowOpenedEvent& e)
    {
        auto newWindow = e.GetWindow();
        if (!newWindow)
        {
            return;
        }

        auto newRenderer = _rendererFactory->Create();
        _windows.push_back(newWindow);
        _renderers.push_back(newRenderer);
        for (const auto& stage : _openStages)
        {
            QueueStageUpload(stage);
        }

        // Init viewport size
        RenderCommandBuffer renderBuffer = {};
        auto newViewport = Viewport({ 0, 0 }, newWindow->GetSize());
        renderBuffer.Commands.emplace_back(RenderCommandType::SetViewport, newViewport);
        newRenderer->Process(renderBuffer);
    }

    void Rendering::OnWindowClosed(const WindowClosedEvent& e)
    {
        auto closedWindow = e.GetWindow();
        const auto renderer = GetRenderer(closedWindow);
        if (!renderer)
        {
            return;
        }

        const auto it = std::find(_windows.begin(), _windows.end(), closedWindow);
        if (it != _windows.end())
        {
            _windows.erase(it);
        }

        const auto rendererIt = std::find(_renderers.begin(), _renderers.end(), renderer);
        if (rendererIt != _renderers.end())
        {
            _renderers.erase(rendererIt);
        }
    }

    void Rendering::OnWindowResized(const WindowResizedEvent& e)
    {
        const auto resizedWindow = e.GetWindow();
        const auto renderer = GetRenderer(resizedWindow);
        if (!renderer)
        {
            return;
        }

        RenderCommandBuffer renderBuffer = {};
        renderBuffer.Commands.emplace_back(RenderCommandType::SetViewport, Viewport({ 0, 0 }, resizedWindow->GetSize()));
        renderer->Process(renderBuffer);
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

