#include "Tbx/Runtime/PCH.h"
#include "Tbx/Runtime/Render Pipeline/RenderProcessor.h"
#include "Tbx/Runtime/Render Pipeline/RenderPipeline.h"
#include "Tbx/Runtime/Render Pipeline/RenderQueue.h"
#include "Tbx/Runtime/Events/RenderEvents.h"
#include "Tbx/Runtime/Events/WindowEvents.h"
#include "Tbx/Runtime/Windowing/WindowManager.h"
#include <Tbx/Core/Events/EventCoordinator.h>
#include <Tbx/Core/Plugins/PluginServer.h>
#include <memory>

namespace Tbx
{
    void RenderPipeline::SetContext(const std::shared_ptr<Playspace>& playspace)
    {
        _currentPlayspace = playspace;

        auto preProcessBatch = _renderProcessor.PreProcess(playspace);
        _renderQueue.Push(preProcessBatch);

        ProcessNextBatch();
    }

    void RenderPipeline::SetVSyncEnabled(bool enabled)
    {
        _vsyncEnabled = enabled;

        SetVSyncRequest request(enabled);
        EventCoordinator::Send(request);
    }

    bool RenderPipeline::IsVSyncEnabled() const
    {
        return _vsyncEnabled;
    }

    void RenderPipeline::Clear() const
    {
        ClearScreenRequest request;
        EventCoordinator::Send(request);
    }

    void RenderPipeline::Flush()
    {
        FlushRendererRequest request;
        EventCoordinator::Send(request);
        _renderQueue.Clear();
    }

    void RenderPipeline::ProcessNextBatch()
    {
        if (!_renderQueue.IsEmpty())
        {
            const auto& batch = _renderQueue.Peek();
            auto request = RenderFrameRequest(batch);
            EventCoordinator::Send(request);
            _renderQueue.Pop();
        }
    }

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
}
