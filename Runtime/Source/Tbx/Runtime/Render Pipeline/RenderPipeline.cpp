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
        _worldMainPlayspaceChangedEventId = EventCoordinator::Subscribe<WorldMainPlayspaceChangedEvent>(TBX_BIND_FN(OnMainPlayspaceChangedEvent));

        ///////// TESTING /////////////

        //auto renderBatch = RenderBatch();

        //// Compile material
        //_redMat = std::make_shared<Material>();
        ////_redMat->SetColor(Colors::Red);
        //const auto& renderMatData = RenderData(RenderCommand::CompileMaterial, _redMat);
        //renderBatch.Add(renderMatData);

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
        EventCoordinator::Unsubscribe<WorldMainPlayspaceChangedEvent>(_worldMainPlayspaceChangedEventId);
        Flush();
    }

    void RenderPipeline::OnUpdate()
    {
        ///////// TESTING /////////////

        //auto renderBatch = RenderBatch();

        //const auto& renderMaterialData = RenderData(RenderCommand::SetMaterial, _redMat);
        //renderBatch.Add(renderMaterialData);

        //const auto& sqaureMeshVerts =
        //{
        //    Vertex(
        //        Vector3(-0.5f, -0.5f, 0.0f),    // Position
        //        Vector3(0.0f, 0.0f, 0.0f),      // Normal
        //        Vector2I(0.0f, 0.0f),           // Texture coordinates
        //        Color(0.0f, 0.0f, 0.0f, 1.0f)), // Color

        //    Vertex(
        //        Vector3(0.5f, -0.5f, 0.0f),     // Position
        //        Vector3(0.0f, 0.0f, 0.0f),      // Normal
        //        Vector2I(1.0f, 0.0f),           // Texture coordinates
        //        Color(0.0f, 0.0f, 0.0f, 1.0f)), // Color

        //    Vertex(
        //        Vector3(0.5f, 0.5f, 0.0f),		 // Position
        //        Vector3(0.0f, 0.0f, 0.0f),      // Normal
        //        Vector2I(1.0f, 1.0f),           // Texture coordinates
        //        Color(0.0f, 0.0f, 0.0f, 1.0f)), // Color

        //    Vertex(
        //        Vector3(-0.5f, 0.5f, 0.0f),     // Position
        //        Vector3(0.0f, 0.0f, 0.0f),      // Normal
        //        Vector2I(0.0f, 1.0f),           // Texture coordinates
        //        Color(0.0f, 0.0f, 0.0f, 1.0f))  // Color
        //};
        //const std::vector<uint32>& squareMeshIndices = { 0, 1, 2, 2, 3, 0 };
        //auto squareMesh = std::make_shared<Mesh>(sqaureMeshVerts, squareMeshIndices);
        //const auto& renderMeshData = RenderData(RenderCommand::RenderMesh, squareMesh);
        //renderBatch.Add(renderMeshData);

        ////const auto& shaderData = ShaderData("transformUni", Tbx::Mat4x4::FromPosition(Vector3::Zero()), Tbx::ShaderDataType::Mat4);
        ////const auto& renderShaderData = RenderData(RenderCommand::UploadMaterialShaderData, shaderData);
        ////renderBatch.Add(renderShaderData);

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

        if (World::GetMainPlayspace() == nullptr) return;

        const auto& playspace = World::GetMainPlayspace();
        auto nextBatch = _renderProcessor.Process(playspace);
        _renderQueue.Push(nextBatch);


        ProcessNextBatch();
    }

    void RenderPipeline::OnMainPlayspaceChangedEvent(const WorldMainPlayspaceChangedEvent& e)
    {
        const auto& playspace = e.GetNewMainPlayspace();
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
