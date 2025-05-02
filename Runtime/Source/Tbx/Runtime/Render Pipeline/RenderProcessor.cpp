#include "Tbx/Runtime/PCH.h"
#include "Tbx/Runtime/Render Pipeline/RenderProcessor.h"
#include "Tbx/Runtime/Windowing/WindowManager.h"
#include <Tbx/Core/Rendering/Shader.h>
#include <Tbx/Core/Rendering/Material.h>
#include <Tbx/Core/Rendering/Camera.h>
#include <Tbx/Core/Rendering/Mesh.h>
#include <Tbx/Core/Math/Transform.h>

namespace Tbx
{
    const RenderBatch& RenderProcessor::PreProcess(const std::shared_ptr<PlaySpace>& playspace)
    {
        _currBatch.Clear();

        std::vector<Toy> _commands = {};
        for (const auto& toy : PlayspaceView<Material, Camera, Transform>(playspace))
        {
            PreProcessToy(toy, playspace);
        }

        _currBatch.Sort();

        return _currBatch;
    }

    const RenderBatch& RenderProcessor::Process(const std::shared_ptr<PlaySpace>& playspace)
    {
        _currBatch.Clear();

        for (const auto& toy : PlayspaceView<Transform, Material, Mesh, Camera>(playspace))
        {
            ProcessToy(toy, playspace);
        }

        _currBatch.Sort();

        return _currBatch;
    }

    void RenderProcessor::PreProcessToy(const Toy& toy, const std::shared_ptr<PlaySpace>& playspace)
    {
        // NOTE: Order is important here!

        // Preprocess materials to upload textures and shaders to GPU
        if (playspace->HasBlockOn<Material>(toy))
        {
            auto& material = playspace->GetBlockOn<Material>(toy);
            _currBatch.Emplace(RenderCommand::CompileMaterial, material);
            _currBatch.Emplace(RenderCommand::SetMaterial, material);
        }

        // Camera block, upload the camera data
        if (playspace->HasBlockOn<Camera>(toy))
        {
            auto& camera = playspace->GetBlockOn<Camera>(toy);
            // Update cameras perspective based on MainWindows view

            // TODO: have camera know what window it should be on, and have it listen to events there.
            const auto& mainWindow = WindowManager::GetMainWindow();
            const auto mainWindowSize = mainWindow.lock()->GetSize();
            const auto aspectRatio = mainWindowSize.GetAspectRatio();
            camera.SetAspect(aspectRatio);

            if (playspace->HasBlockOn<Transform>(toy))
            {
                // Use the transform block's position and rotation
                auto& cameraTransform = playspace->GetBlockOn<Transform>(toy);
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
    }

    void RenderProcessor::ProcessToy(const Toy& toy, const std::shared_ptr<PlaySpace>& playspace)
    {
        // NOTE: Order is important here!
        
        // Mesh block, upload the mesh data
        if (playspace->HasBlockOn<Mesh>(toy))
        {
            auto& mesh = playspace->GetBlockOn<Mesh>(toy);
            _currBatch.Emplace(RenderCommand::RenderMesh, Mesh::MakeQuad());
        }

        // Transform block, upload the transform data
        if (playspace->HasBlockOn<Transform>(toy))
        {
            auto& transform = playspace->GetBlockOn<Transform>(toy);
            const auto& shaderData = ShaderData(
                "transformUni",
                Mat4x4::FromTRS(transform.Position, transform.Rotation, transform.Scale),
                ShaderDataType::Mat4);
            _currBatch.Emplace(RenderCommand::UploadMaterialShaderData, shaderData);
        }

        // Camera block, upload the camera data
        if (playspace->HasBlockOn<Camera>(toy))
        {
            auto& camera = playspace->GetBlockOn<Camera>(toy);
            // Update cameras perspective based on MainWindows view
            
            // TODO: have camera know what window it should be on, and have it listen to events there.
            const auto& mainWindow = WindowManager::GetMainWindow();
            const auto mainWindowSize = mainWindow.lock()->GetSize();
            const auto aspectRatio = mainWindowSize.GetAspectRatio();
            camera.SetAspect(aspectRatio);

            if (playspace->HasBlockOn<Transform>(toy))
            {
                // Use the transform block's position and rotation
                auto& cameraTransform = playspace->GetBlockOn<Transform>(toy);
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
        if (playspace->HasBlockOn<Material>(toy))
        {
            auto& material = playspace->GetBlockOn<Material>(toy);
            _currBatch.Emplace(RenderCommand::SetMaterial, material);
        }
    }
}
