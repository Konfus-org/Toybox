#include "Tbx/Runtime/PCH.h"
#include "Tbx/Runtime/Render Pipeline/RenderProcessor.h"
#include "Tbx/Runtime/Render Pipeline/RenderPipeline.h"
#include "Tbx/Runtime/Events/RenderEvents.h"
#include "Tbx/Runtime/Events/WindowEvents.h"
#include "Tbx/Runtime/Windowing/WindowManager.h"
#include <Tbx/Core/Rendering/RenderingAPI.h>
#include <Tbx/Core/Events/EventCoordinator.h>
#include <Tbx/Core/Plugins/PluginServer.h>
#include <Tbx/Core/TBS/World.h>
#include <memory>

namespace Tbx
{
    bool RenderPipeline::IsOverlay()
    {
        return false;
    }


    Camera _cam = Camera();
    static std::shared_ptr<Material> _redMat = nullptr;

    void RenderPipeline::OnAttach()
    {
        _worldPlayspaceChangedEventId = EventCoordinator::Subscribe<WorldPlayspacesAddedEvent>(TBX_BIND_FN(OnPlayspaceChangedEvent));
    }

    void RenderPipeline::OnDetach()
    {
        EventCoordinator::Unsubscribe<WorldPlayspacesAddedEvent>(_worldPlayspaceChangedEventId);
        Flush();
    }

    void RenderPipeline::OnUpdate()
    {
        for (const auto& playspace : World::GetPlayspaces())
        {
            auto nextBatch = _renderProcessor.Process(playspace);
            _renderQueue.Push(nextBatch);
        }


        ProcessNextBatch();
    }

    void RenderPipeline::OnPlayspaceChangedEvent(const WorldPlayspacesAddedEvent& e)
    {
        for (const auto& playspaceId : e.GetNewPlayspaces())
        {
            auto nextBatch = _renderProcessor.PreProcess(World::GetPlayspace(playspaceId));
            _renderQueue.Push(nextBatch);
        }

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
