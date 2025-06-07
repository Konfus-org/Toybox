#include "Tbx/Runtime/PCH.h"
#include "Tbx/Runtime/Render Pipeline/RenderProcessor.h"
#include "Tbx/Runtime/Render Pipeline/RenderPipeline.h"
#include "Tbx/Runtime/Events/RenderEvents.h"
#include <Tbx/Core/Events/EventCoordinator.h>
#include <Tbx/Core/Plugins/PluginServer.h>
#include <Tbx/Core/TBS/World.h>
#include <memory>

///////// TESTING /////////////

//#include "Tbx/Core/Rendering/Material.h"
//#include "Tbx/Core/Rendering/Mesh.h"
//#include "Tbx/Core/Rendering/Shader.h"
//#include "Tbx/Runtime/Windowing/WindowManager.h"
//static Camera _cam = Camera();
//static std::shared_ptr<Material> _redMat = nullptr;
//static std::shared_ptr<Mesh> _squareMesh = nullptr;

///////// TESTING /////////////

namespace Tbx
{


    bool RenderPipeline::IsOverlay()
    {
        return false;
    }

    void RenderPipeline::OnAttach()
    {
        _worldPlayspaceChangedEventId = EventCoordinator::Subscribe<OpenPlaySpacesRequest>(TBX_BIND_FN(OnOpenPlayspaceRequest));
    }

    void RenderPipeline::OnDetach()
    {
        EventCoordinator::Unsubscribe<WorldPlaySpacesAddedEvent>(_worldPlayspaceChangedEventId);
        Flush();
    }

    void RenderPipeline::OnUpdate()
    {
        for (const auto& playSpace : World::GetPlaySpaces())
        {
            auto batches = RenderProcessor::Process(playSpace);
            for (const auto& batch : batches)
            {
                _renderQueue.Push(batch);
            }
        }

        ProcessNextBatch();
    }

    void RenderPipeline::OnOpenPlayspaceRequest(OpenPlaySpacesRequest& e)
    {
        for (const auto& playSpaceId : e.GetPlaySpacesToOpen())
        {
            auto batches = RenderProcessor::PreProcess(World::GetPlayspace(playSpaceId));
            for (const auto& batch : batches)
            {
                _renderQueue.Push(batch);
            }
        }

        e.IsHandled = true;

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
        while (!_renderQueue.IsEmpty())
        {
            const auto& batch = _renderQueue.Peek();
            auto request = RenderFrameRequest(batch);
            EventCoordinator::Send(request);

            TBX_ASSERT(request.IsHandled, "Render frame request was not handled. Is a renderer created and listening?");

            _renderQueue.Pop();
        }
    }
}
