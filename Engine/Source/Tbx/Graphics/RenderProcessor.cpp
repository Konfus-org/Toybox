#include "Tbx/PCH.h"
#include "Tbx/Graphics/RenderProcessor.h"
#include "Tbx/Graphics/Buffers.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Material.h"
#include "Tbx/Graphics/Camera.h"
#include "Tbx/Graphics/Mesh.h"
#include "Tbx/Graphics/Model.h"
#include "Tbx/Math/Transform.h"

namespace Tbx
{
    FrameBuffer RenderProcessor::PreProcess(const std::vector<std::shared_ptr<Playspace>>& playSpaces)
    {
        FrameBuffer buffer = {};

        for (const auto& playspace : playSpaces)
        {
            // We only need to pre-process materials to upload them to the GPU
            for (const auto& toy : PlayspaceView<Material>(playspace))
            {
                PreProcessToy(toy, playspace, buffer);
            }
        }

        return buffer;
    }

    FrameBuffer RenderProcessor::Process(const std::vector<std::shared_ptr<Playspace>>& playSpaces)
    {
        FrameBuffer buffer = {};
        for (const auto& playspace : playSpaces)
        {
            for (const auto& toy : PlayspaceView(playspace))
            {
                ProcessToy(toy, playspace, buffer);
            }
        }

        return buffer;
    }

    void RenderProcessor::PreProcessToy(const Toy& toy, const std::shared_ptr<Playspace>& playSpace, FrameBuffer& buffer)
    {
        // Preprocess materials to upload textures and shaders to GPU
        if (playSpace->HasBlockOn<Material>(toy))
        {
            auto& material = playSpace->GetBlockOn<Material>(toy);

            // Check if we already added this material
            auto& existingCompileMatCmds = buffer.GetCommands();
            for (const auto& cmd : existingCompileMatCmds)
            {
                if (cmd.GetType() != DrawCommandType::CompileMaterial) continue;

                const auto& materialToCompile = std::any_cast<const Material&>(cmd.GetPayload());
                if (materialToCompile.GetId() == material)
                {
                    return;
                }
            }

            // New material, add to buffer
            buffer.Emplace(DrawCommandType::CompileMaterial, material);
        }
    }

    void RenderProcessor::ProcessToy(const Toy& toy, const std::shared_ptr<Playspace>& playSpace, FrameBuffer& buffer)
    {
        // NOTE: Order matters here!!!
        
        // Material block, upload the material data
        if (playSpace->HasBlockOn<Material>(toy))
        {
            auto& material = playSpace->GetBlockOn<Material>(toy);
            buffer.Emplace(DrawCommandType::SetMaterial, material);
        }

        // Model block, upload model data
        if (playSpace->HasBlockOn<Model>(toy))
        {
            const auto& model = playSpace->GetBlockOn<Model>(toy);
            buffer.Emplace(DrawCommandType::SetMaterial, model.GetMaterial());
            buffer.Emplace(DrawCommandType::DrawMesh, model.GetMesh());
        }

        // Transform block, upload the transform data
        if (playSpace->HasBlockOn<Transform>(toy))
        {
            const auto& transform = playSpace->GetBlockOn<Transform>(toy);
            const auto transformData = ShaderData(
                "transformUni",
                Mat4x4::FromTRS(transform.Position, transform.Rotation, transform.Scale),
                ShaderDataType::Mat4);
            buffer.Emplace(DrawCommandType::UploadMaterialData, transformData);
        }
        
        // Mesh block, upload the mesh data
        if (playSpace->HasBlockOn<Mesh>(toy))
        {
            auto& mesh = playSpace->GetBlockOn<Mesh>(toy);
            buffer.Emplace(DrawCommandType::DrawMesh, mesh);
        }

        // Camera block, upload the camera data
        if (playSpace->HasBlockOn<Camera>(toy))
        {
            auto& camera = playSpace->GetBlockOn<Camera>(toy);
            if (playSpace->HasBlockOn<Transform>(toy))
            {
                // Use the transform block's position and rotation
                const auto& cameraTransform = playSpace->GetBlockOn<Transform>(toy);
                const auto viewProjMatrix = Camera::CalculateViewProjectionMatrix(
                    cameraTransform.Position, cameraTransform.Rotation, camera.GetProjectionMatrix());
                const auto shaderData = ShaderData("viewProjectionUni", viewProjMatrix, ShaderDataType::Mat4);
                buffer.Emplace(DrawCommandType::UploadMaterialData, shaderData);
            }
            else
            {
                // No transform block, use default camera position and rotation
                const auto viewProjMatrix = Camera::CalculateViewProjectionMatrix(
                    Constants::Vector3::Zero, Constants::Quaternion::Identity, camera.GetProjectionMatrix());
                const auto shaderData = ShaderData("viewProjectionUni", viewProjMatrix, ShaderDataType::Mat4);
                buffer.Emplace(DrawCommandType::UploadMaterialData, shaderData);
            }
        }
    }
}
