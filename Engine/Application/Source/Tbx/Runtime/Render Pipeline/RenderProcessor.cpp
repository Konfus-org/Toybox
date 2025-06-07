#include "Tbx/Runtime/PCH.h"
#include "Tbx/Runtime/Render Pipeline/RenderProcessor.h"
#include "Tbx/Runtime/Windowing/WindowManager.h"
#include <Tbx/Core/Rendering/Shader.h>
#include <Tbx/Core/Rendering/Material.h>
#include <Tbx/Core/Rendering/Camera.h>
#include <Tbx/Core/Rendering/Mesh.h>
#include <Tbx/Math/Transform.h>

namespace Tbx
{
    std::vector<RenderBatch> RenderProcessor::PreProcess(const std::shared_ptr<PlaySpace>& playSpace)
    {
        std::unordered_map<uint64, RenderBatch> batchesOut = {};

        for (const auto& toy : PlayspaceView<Material, Camera, Transform>(playSpace))
        {
            PreProcessToy(toy, playSpace, batchesOut);
        }

        std::vector<RenderBatch> batches = {};
        for (auto& batch : batchesOut | std::views::values)
        {
            batches.push_back(batch);
        }

        return batches;
    }

    std::vector<RenderBatch> RenderProcessor::Process(const std::shared_ptr<PlaySpace>& playSpace)
    {
        std::unordered_map<uint64, RenderBatch> batchesOut = {};

        for (const auto& toy : PlayspaceView<Transform, Material, Mesh, Camera>(playSpace))
        {
            ProcessToy(toy, playSpace, batchesOut);
        }

        std::vector<RenderBatch> batches = {};
        for (auto& batch : batchesOut | std::views::values)
        {
            batches.push_back(batch);
        }

        return batches;
    }

    void RenderProcessor::PreProcessToy(const Toy& toy, const std::shared_ptr<PlaySpace>& playSpace, std::unordered_map<uint64, RenderBatch>& batchesOut)
    {
        // Preprocess materials to upload textures and shaders to GPU
        if (playSpace->HasBlockOn<Material>(toy))
        {
            auto& material = playSpace->GetBlockOn<Material>(toy);
            const auto& matShaderId = material.GetShader().Id;
            const auto& matTextureId = material.GetTextures().front().Id;

            // We batch by shader
            if (!batchesOut.contains(matShaderId))
            {
                batchesOut[matShaderId] = RenderBatch();
                batchesOut[matShaderId].Emplace(RenderCommand::CompileMaterial, material);
            }

            // If textures differ we batch by them
            if (!batchesOut.contains(matTextureId))
            {
                batchesOut[matTextureId] = RenderBatch();
                batchesOut[matTextureId].Emplace(RenderCommand::UploadMaterialsTextures, material);
            }
        }
    }

    void RenderProcessor::ProcessToy(const Toy& toy, const std::shared_ptr<PlaySpace>& playSpace, std::unordered_map<uint64, RenderBatch>& batchesOut)
    {
        // NOTE: Order matters here!!!

        UID batchId = -1; // No material batch id...
        auto thisBatch = RenderBatch();
        
        // Material block, upload the material data
        if (playSpace->HasBlockOn<Material>(toy))
        {
            auto& material = playSpace->GetBlockOn<Material>(toy);
            batchId = material.GetShader().Id;
            thisBatch.Emplace(RenderCommand::SetMaterial, material);
        }

        // Transform block, upload the transform data
        if (playSpace->HasBlockOn<Transform>(toy))
        {
            auto& transform = playSpace->GetBlockOn<Transform>(toy);
            const auto shaderData = ShaderData(
                "transformUni",
                Mat4x4::FromTRS(transform.Position, transform.Rotation, transform.Scale),
                ShaderDataType::Mat4);
            thisBatch.Emplace(RenderCommand::UploadMaterialShaderData, shaderData);
        }
        
        // Mesh block, upload the mesh data
        if (playSpace->HasBlockOn<Mesh>(toy))
        {
            auto& mesh = playSpace->GetBlockOn<Mesh>(toy);
            thisBatch.Emplace(RenderCommand::RenderMesh, Mesh::MakeQuad());
        }

        // Camera block, upload the camera data
        if (playSpace->HasBlockOn<Camera>(toy))
        {
            auto& camera = playSpace->GetBlockOn<Camera>(toy);
            // Update cameras perspective based on MainWindows view
            
            // TODO: have camera know what window it should be on, and have it listen to events there.
            const auto& mainWindow = WindowManager::GetMainWindow();
            const auto mainWindowSize = mainWindow.lock()->GetSize();
            const auto aspectRatio = mainWindowSize.GetAspectRatio();
            camera.SetAspect(aspectRatio);

            if (playSpace->HasBlockOn<Transform>(toy))
            {
                // Use the transform block's position and rotation
                auto& cameraTransform = playSpace->GetBlockOn<Transform>(toy);
                const auto viewProjMatrix = Camera::CalculateViewProjectionMatrix(
                    cameraTransform.Position, cameraTransform.Rotation, camera.GetProjectionMatrix());
                const auto shaderData = ShaderData("viewProjectionUni", viewProjMatrix, ShaderDataType::Mat4);
                thisBatch.Emplace(RenderCommand::UploadMaterialShaderData, shaderData);
            }
            else
            {
                // No transform block, use default camera position and rotation
                const auto viewProjMatrix = Camera::CalculateViewProjectionMatrix(
                    Constants::Vector3::Zero, Constants::Quaternion::Identity, camera.GetProjectionMatrix());
                const auto shaderData = ShaderData("viewProjectionUni", viewProjMatrix, ShaderDataType::Mat4);
                thisBatch.Emplace(RenderCommand::UploadMaterialShaderData, shaderData);
            }
        }

        // Sort and push into batch
        thisBatch.Sort();
        if (!batchesOut.contains(batchId))
        {
            batchesOut[batchId] = thisBatch;
        }
        else
        {
            for (const auto& batchItem : thisBatch)
            {
                batchesOut[batchId].Emplace(batchItem);
            }
        }
    }
}
