#include "Tbx/Runtime/PCH.h"
#include "Tbx/Runtime/Render Pipeline/RenderProcessor.h"

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
        auto material = toy->GetBlock<Material>();
        if (material)
        {
            // Preprocess materials to upload textures and shaders to GPU
            _currBatch.Emplace(RenderCommand::CompileMaterial, material);
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
        // Material block, upload the material data
        if (auto material = toy->GetBlock<Material>(); material != nullptr)
        {
            _currBatch.Emplace(RenderCommand::SetMaterial, material);
        }

        // Camera block, upload the camera data
        if (auto camera = toy->GetBlock<Camera>(); camera != nullptr)
        {
            if (auto cameraTransform = toy->GetBlock<Transform>(); cameraTransform != nullptr)
            {
                // Use the transform block's position and rotation
                const auto& shaderData = Tbx::ShaderData(
                    "viewProjection",
                    Camera::CalculateViewProjectionMatrix(cameraTransform->Position, cameraTransform->Rotation, camera->GetProjectionMatrix()),
                    Tbx::ShaderDataType::Mat4);
                _currBatch.Emplace(Tbx::RenderCommand::UploadMaterialShaderData, shaderData);
            }
            else
            {
                // No transform block, use default camera position and rotation
                const auto& shaderData = Tbx::ShaderData(
                    "viewProjection",
                    Camera::CalculateViewProjectionMatrix(Vector3::Zero(), Quaternion::Identity(), camera->GetProjectionMatrix()),
                    Tbx::ShaderDataType::Mat4);
                _currBatch.Emplace(Tbx::RenderCommand::UploadMaterialShaderData, shaderData);
            }
        }

        // Transform block, upload the transform data
        if (auto transform = toy->GetBlock<Transform>(); transform != nullptr)
        {
            const auto& shaderData = Tbx::ShaderData(
                "transform",
                Tbx::Mat4x4::FromTRS(transform->Position, transform->Rotation, transform->Scale),
                Tbx::ShaderDataType::Mat4);
            _currBatch.Emplace(Tbx::RenderCommand::UploadMaterialShaderData, shaderData);
        }

        // Mesh block, upload the mesh data
        if (auto mesh = toy->GetBlock<Mesh>(); mesh != nullptr)
        {
            _currBatch.Emplace(RenderCommand::RenderMesh, mesh);
        }
    }
}
