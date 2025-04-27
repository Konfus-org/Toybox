#include "Tbx/Runtime/PCH.h"
#include "Tbx/Runtime/Render Pipeline/RenderProcessor.h"
#include "Tbx/Runtime/Windowing/WindowManager.h"

namespace Tbx
{
    const RenderBatch& RenderProcessor::PreProcess(const std::shared_ptr<Playspace>& playspace)
    {
        _currBatch.Clear();
        PreProcessBoxes(playspace->GetBoxes());
        return _currBatch;
    }

    const RenderBatch& RenderProcessor::Process(const std::shared_ptr<Playspace>& playspace)
    {
        _currBatch.Clear();
        ProcessBoxes(playspace->GetBoxes());
        return _currBatch;
    }

    void RenderProcessor::PreProcessBoxes(const std::vector<std::shared_ptr<Box>>& boxes)
    {
        for (const auto& box : boxes)
        {
            const auto& itemsInBox = box->GetAllItems();
            for (const auto& item : itemsInBox)
            {
                if (const auto& toy = std::dynamic_pointer_cast<Toy>(item))
                {
                    PreProcessToy(toy);
                }
                else if (const auto& boxItem = std::dynamic_pointer_cast<Box>(item))
                {
                    PreProcessBoxes({ boxItem });
                }
            }
        }
    }

    void RenderProcessor::PreProcessToy(const std::shared_ptr<Toy>& toy)
    {
        // NOTE: Order is important here!
        
        // Camera block, upload the camera data
        if (auto camera = toy->GetBlock<Camera>(); camera != nullptr)
        {
            // Update cameras perspective based on MainWindows view
            // TODO: have camera know what window it should be on, and have it listen to events there.
            const auto& mainWindow = WindowManager::GetMainWindow();
            const auto mainWindowSize = mainWindow.lock()->GetSize();
            const auto aspectRatio = mainWindowSize.GetAspectRatio();
            camera->SetAspect(aspectRatio);

            if (auto cameraTransform = toy->GetBlock<Transform>(); cameraTransform != nullptr)
            {
                // Use the transform block's position and rotation
                const auto& viewProjMatrix = Camera::CalculateViewProjectionMatrix(cameraTransform->Position, cameraTransform->Rotation, camera->GetProjectionMatrix());
                const auto& shaderData = ShaderData("viewProjectionUni", viewProjMatrix, ShaderDataType::Mat4);
                _currBatch.Emplace(RenderCommand::UploadMaterialShaderData, shaderData);
            }
            else
            {
                // No transform block, use default camera position and rotation
                const auto& viewProjMatrix = Camera::CalculateViewProjectionMatrix(Vector3::Zero(), Quaternion::Identity(), camera->GetProjectionMatrix());
                const auto& shaderData = ShaderData("viewProjectionUni", viewProjMatrix, ShaderDataType::Mat4);
                _currBatch.Emplace(RenderCommand::UploadMaterialShaderData, shaderData);
            }
        }

        // Preprocess materials to upload textures and shaders to GPU
        if (auto material = toy->GetBlock<Material>(); material != nullptr)
        {
            _currBatch.Emplace(RenderCommand::CompileMaterial, material);
            _currBatch.Emplace(RenderCommand::SetMaterial, material);
        }
    }

    void RenderProcessor::ProcessBoxes(const std::vector<std::shared_ptr<Box>>& boxes)
    {
        for (const auto& box : boxes)
        {
            const auto& itemsInBox = box->GetAllItems();
            for (const auto& item : itemsInBox)
            {
                if (const auto& toy = std::dynamic_pointer_cast<Toy>(item))
                {
                    ProcessToy(toy);
                }
                else if (const auto& boxItem = std::dynamic_pointer_cast<Box>(item))
                {
                    ProcessBoxes({ boxItem });
                }
            }
        }
    }

    void RenderProcessor::ProcessToy(const std::shared_ptr<Toy>& toy)
    {
        // NOTE: Order is important here!
        
        // Mesh block, upload the mesh data
        if (auto mesh = toy->GetBlock<Mesh>(); mesh != nullptr)
        {
            _currBatch.Emplace(RenderCommand::RenderMesh, mesh);
        }

        // Transform block, upload the transform data
        if (auto transform = toy->GetBlock<Transform>(); transform != nullptr)
        {
            const auto& shaderData = ShaderData(
                "transformUni",
                Mat4x4::FromTRS(transform->Position, transform->Rotation, transform->Scale),
                ShaderDataType::Mat4);
            _currBatch.Emplace(RenderCommand::UploadMaterialShaderData, shaderData);
        }

        // Camera block, upload the camera data
        if (auto camera = toy->GetBlock<Camera>(); camera != nullptr)
        {
            // Update cameras perspective based on MainWindows view
            
            // TODO: have camera know what window it should be on, and have it listen to events there.
            const auto& mainWindow = WindowManager::GetMainWindow();
            const auto mainWindowSize = mainWindow.lock()->GetSize();
            const auto aspectRatio = mainWindowSize.GetAspectRatio();
            camera->SetAspect(aspectRatio);

            if (auto cameraTransform = toy->GetBlock<Transform>(); cameraTransform != nullptr)
            {
                // Use the transform block's position and rotation
                const auto& viewProjMatrix = Camera::CalculateViewProjectionMatrix(cameraTransform->Position, cameraTransform->Rotation, camera->GetProjectionMatrix());
                const auto& shaderData = ShaderData("viewProjectionUni", viewProjMatrix, ShaderDataType::Mat4);
                _currBatch.Emplace(RenderCommand::UploadMaterialShaderData, shaderData);
            }
            else
            {
                // No transform block, use default camera position and rotation
                const auto& viewProjMatrix = Camera::CalculateViewProjectionMatrix(Vector3::Zero(), Quaternion::Identity(), camera->GetProjectionMatrix());
                const auto& shaderData = ShaderData("viewProjectionUni", viewProjMatrix, ShaderDataType::Mat4);
                _currBatch.Emplace(RenderCommand::UploadMaterialShaderData, shaderData);
            }
        }

        // Material block, upload the material data
        if (auto material = toy->GetBlock<Material>(); material != nullptr)
        {
            _currBatch.Emplace(RenderCommand::SetMaterial, material);
        }
    }
}
