#include "Tbx/Runtime/PCH.h"
#include "Tbx/Runtime/Render Pipeline/RenderProcessor.h"
#include "Tbx/Runtime/Render Pipeline/RenderPipeline.h"
#include "Tbx/Runtime/Events/RenderEvents.h"
#include "Tbx/Runtime/Events/WindowEvents.h"
#include "Tbx/Runtime/Windowing/WindowManager.h"
#include <Tbx/Core/Rendering/RenderQueue.h>
#include <Tbx/Core/Events/EventCoordinator.h>
#include <Tbx/Core/Plugins/PluginServer.h>
#include <memory>

namespace Tbx
{
    bool RenderPipeline::IsOverlay()
    {
        return false;
    }

    void RenderPipeline::OnAttach()
    {
        // Do nothing...
    }

    void RenderPipeline::OnDetach()
    {
        _currentPlayspace.reset();
        Flush();
    }

    void RenderPipeline::OnUpdate()
    {
        auto nextBatch = _renderProcessor.Process(_currentPlayspace);
        _renderQueue.Push(nextBatch);
        ProcessNextBatch();
    }

    void RenderPipeline::SetContext(const std::shared_ptr<Playspace>& playspace)
    {
        _currentPlayspace = playspace;

        auto preProcessBatch = _renderProcessor.PreProcess(playspace);
        _renderQueue.Push(preProcessBatch);

        ProcessNextBatch();
    }

    void RenderPipeline::Clear() const
    {
        ClearScreenRequest request;
        EventCoordinator::Send(request);

        TBX_ASSERT(request.IsHandled, "Clear screen request was not handled. Is a renderer created and listening?");
    }

    void RenderPipeline::Flush()
    {
        Clear();

        FlushRendererRequest request;
        EventCoordinator::Send(request);

        TBX_ASSERT(request.IsHandled, "Flush request was not handled. Is a renderer created and listening?");

        _renderQueue.Clear();
    }

    void RenderPipeline::ProcessNextBatch()
    {
        if (!_renderQueue.IsEmpty())
        {
            const auto& batch = _renderQueue.Peek();
            auto request = RenderFrameRequest(batch);
            EventCoordinator::Send(request);

            TBX_ASSERT(request.IsHandled, "Render frame request was not handled. Is a renderer created and listening?");

            _renderQueue.Pop();
        }
    }
}
