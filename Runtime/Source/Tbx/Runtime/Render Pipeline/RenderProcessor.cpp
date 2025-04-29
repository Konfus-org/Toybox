#include "Tbx/Runtime/PCH.h"
#include "Tbx/Runtime/Render Pipeline/RenderProcessor.h"
#include "Tbx/Runtime/Windowing/WindowManager.h"

namespace Tbx
{
    const RenderBatch& RenderProcessor::PreProcess(const std::weak_ptr<Playspace>& playspace)
    {
        _currBatch.Clear();

        for (const auto& toy : PlayspaceView<Material>(playspace))
        {
            PreProcessToy(toy, playspace);
        }

        return _currBatch;
    }

    const RenderBatch& RenderProcessor::Process(const std::weak_ptr<Playspace>& playspace)
    {
        _currBatch.Clear();

        for (const auto& toy : PlayspaceView<Transform, Material, Mesh, Camera>(playspace))
        {
            ProcessToy(toy, playspace);
        }

        return _currBatch;
    }

    void RenderProcessor::PreProcessToy(const Toy& toy, const std::weak_ptr<Playspace>& playspace)
    {
        // NOTE: Order is important here!

        // Preprocess materials to upload textures and shaders to GPU
        if (playspace.lock()->HasBlockOn<Material>(toy))
        {
            auto& material = playspace.lock()->GetBlockOn<Material>(toy);
            _currBatch.Emplace(RenderCommand::CompileMaterial, material);
            _currBatch.Emplace(RenderCommand::SetMaterial, material);
        }
    }

    void RenderProcessor::ProcessToy(const Toy& toy, const std::weak_ptr<Playspace>& playspace)
    {
        // NOTE: Order is important here!
        
        // Mesh block, upload the mesh data
        if (playspace.lock()->HasBlockOn<Mesh>(toy))
        {
            auto& mesh = playspace.lock()->GetBlockOn<Mesh>(toy);
            _currBatch.Emplace(RenderCommand::RenderMesh, mesh);
        }

        // Transform block, upload the transform data
        if (playspace.lock()->HasBlockOn<Transform>(toy))
        {
            auto& transform = playspace.lock()->GetBlockOn<Transform>(toy);
            const auto& shaderData = ShaderData(
                "transformUni",
                Mat4x4::FromTRS(transform.Position, transform.Rotation, transform.Scale),
                ShaderDataType::Mat4);
            _currBatch.Emplace(RenderCommand::UploadMaterialShaderData, shaderData);
        }

        // Camera block, upload the camera data
        if (playspace.lock()->HasBlockOn<Transform>(toy))
        {
            auto& camera = playspace.lock()->GetBlockOn<Camera>(toy);
            // Update cameras perspective based on MainWindows view
            
            // TODO: have camera know what window it should be on, and have it listen to events there.
            const auto& mainWindow = WindowManager::GetMainWindow();
            const auto mainWindowSize = mainWindow.lock()->GetSize();
            const auto aspectRatio = mainWindowSize.GetAspectRatio();
            camera.SetAspect(aspectRatio);

            if (playspace.lock()->HasBlockOn<Transform>(toy))
            {
                auto& cameraTransform = playspace.lock()->GetBlockOn<Transform>(toy);
                // Use the transform block's position and rotation
                const auto& viewProjMatrix = Camera::CalculateViewProjectionMatrix(cameraTransform.Position, cameraTransform.Rotation, camera.GetProjectionMatrix());
                const auto& shaderData = ShaderData("viewProjectionUni", viewProjMatrix, ShaderDataType::Mat4);
                _currBatch.Emplace(RenderCommand::UploadMaterialShaderData, shaderData);
            }
            else
            {
                // No transform block, use default camera position and rotation
                const auto& viewProjMatrix = Camera::CalculateViewProjectionMatrix(Vector3::Zero(), Quaternion::Identity(), camera.GetProjectionMatrix());
                const auto& shaderData = ShaderData("viewProjectionUni", viewProjMatrix, ShaderDataType::Mat4);
                _currBatch.Emplace(RenderCommand::UploadMaterialShaderData, shaderData);
            }
        }

        // Material block, upload the material data
        if (playspace.lock()->HasBlockOn<Material>(toy))
        {
            auto& material = playspace.lock()->GetBlockOn<Material>(toy);
            _currBatch.Emplace(RenderCommand::SetMaterial, material);
        }
    }
}
