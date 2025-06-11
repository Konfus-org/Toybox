#include "Tbx/Systems/PCH.h"
#include "Tbx/Systems/Rendering/RenderProcessor.h"
#include "Tbx/Systems/Rendering/RenderPipeline.h"
#include "Tbx/Systems/Rendering/RenderEvents.h"
#include "Tbx/Systems/Events/EventCoordinator.h"
#include "Tbx/Systems/Plugins/PluginServer.h"
#include "Tbx/Systems/TBS/World.h"
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
    void RenderPipeline::Initialize()
    {
        // Do nothing for now...
    }

    void RenderPipeline::Shutdown()
    {
        // Do nothing for now...
    }

    void RenderPipeline::Update()
    {
        // Do nothing for now...
    }

    void RenderPipeline::GetPlayspaceReadyForRendering(std::shared_ptr<Playspace> playSpace)
    {
        auto buffer = RenderProcessor::PreProcess(playSpace);

        auto request = RenderFrameRequest(buffer);
        EventCoordinator::Send(request);

        TBX_ASSERT(request.IsHandled, "Render frame request was not handled. Is a renderer created and listening?");
    }

    void RenderPipeline::RenderPlayspace(std::shared_ptr<Playspace> playSpace)
    {
        auto buffer = RenderProcessor::Process(playSpace);

        auto request = RenderFrameRequest(buffer);
        EventCoordinator::Send(request);

        TBX_ASSERT(request.IsHandled, "Render frame request was not handled. Is a renderer created and listening?");
    }
}
