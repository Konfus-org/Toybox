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

        ///////// TESTING /////////////

        //auto renderBatch = RenderBatch();

        //_squareMesh = std::make_shared<Mesh>(Primitives::Quad);

        //// Compile material
        //_redMat = std::make_shared<Material>();
        //_redMat->SetColor(Colors::Red);
        //const auto& renderCompileMatData = RenderData(RenderCommand::CompileMaterial, _redMat);
        //renderBatch.Add(renderCompileMatData);
        //const auto& renderSetMatData = RenderData(RenderCommand::SetMaterial, _redMat);
        //renderBatch.Add(renderSetMatData);

        //// Configure camera
        //const auto& mainWindow = Tbx::WindowManager::GetMainWindow();
        //const auto& mainWindowSize = mainWindow.lock()->GetSize();

        //// Test perspective camera
        //_cam.SetPerspective(45.0f, mainWindowSize.GetAspectRatio(), 0.1f, 100);

        //// Test ortho camera
        //////mainWindowCam->SetOrthagraphic(1, mainWindowSize.AspectRatio(), -1, 10);
        //////mainWindowCam->SetPosition(Tbx::Vector3(0.0f, 0.0f, -1.0f));

        //const auto& shaderProjData =
        //    Tbx::ShaderData("viewProjectionUni",
        //        Camera::CalculateViewProjectionMatrix(
        //            Tbx::Vector3(0.0f, 0.0f, -5.0f), Tbx::Quaternion::FromEuler(Tbx::Vector3(0.0f, 0.0f, 0.0f)), _cam.GetProjectionMatrix()),
        //        Tbx::ShaderDataType::Mat4);
        //const auto& renderCamData = Tbx::RenderData(Tbx::RenderCommand::UploadMaterialShaderData, shaderProjData);
        //renderBatch.Add(renderCamData);

        //_renderQueue.Push(renderBatch);

        //ProcessNextBatch();

        ///////// TESTING /////////////
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
            auto& nextBatch = _renderProcessor.Process(playSpace);
            _renderQueue.Push(nextBatch);
        }

        /////// TESTING /////////////

        //auto renderBatch = RenderBatch();

        //const auto& renderMaterialData = RenderData(RenderCommand::SetMaterial, _redMat);
        //renderBatch.Add(renderMaterialData);

        //const auto& renderMeshData = RenderData(RenderCommand::RenderMesh, _squareMesh);
        //renderBatch.Add(renderMeshData);

        ////const auto& shaderData = ShaderData("transformUni", Tbx::Mat4x4::FromPosition(Vector3::Zero()), Tbx::ShaderDataType::Mat4);
        ////const auto& renderShaderData = RenderData(RenderCommand::UploadMaterialShaderData, shaderData);
        ////renderBatch.Add(renderShaderData);

        //const auto& shaderProjData =
        //    Tbx::ShaderData("viewProjectionUni",
        //        Camera::CalculateViewProjectionMatrix(
        //            Tbx::Vector3(0.0f, 0.0f, -10.0f), Tbx::Quaternion::Identity(), _cam.GetProjectionMatrix()),
        //        Tbx::ShaderDataType::Mat4);
        //const auto& renderCamData = Tbx::RenderData(Tbx::RenderCommand::UploadMaterialShaderData, shaderProjData);
        //renderBatch.Add(renderCamData);

        //_renderQueue.Push(renderBatch);

        /////// TESTING /////////////

        ProcessNextBatch();
    }

    void RenderPipeline::OnOpenPlayspaceRequest(OpenPlaySpacesRequest& e)
    {
        for (const auto& playSpaceId : e.GetPlaySpacesToOpen())
        {
            auto& nextBatch = _renderProcessor.PreProcess(World::GetPlayspace(playSpaceId));
            _renderQueue.Push(nextBatch);
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
