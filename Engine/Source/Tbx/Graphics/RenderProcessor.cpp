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
    FrameBuffer RenderProcessor::PreProcess(const std::vector<std::shared_ptr<Box>>& boxes)
    {
        FrameBuffer buffer = {};

        for (const auto& box : boxes)
        {
            // We only need to pre-process materials to upload them to the GPU
            for (const auto& toy : BoxView<Material>(box))
            {
                PreProcessToy(toy, box, buffer);
            }
        }

        return buffer;
    }

    FrameBuffer RenderProcessor::Process(const std::vector<std::shared_ptr<Box>>& boxes)
    {
        // First we need to nab all the cameras and get their view projection matrices
        FrameBuffer buffer = {};
        for (const auto& box : boxes)
        {
            for (const auto& toy : BoxView<Camera>(box))
            {
                ProcessToy(toy, box, buffer);
            }
        }


        FrameBuffer buffer = {};
        for (const auto& box : boxes)
        {
            for (const auto& toy : BoxView(box))
            {
                ProcessToy(toy, box, buffer);
            }
        }

        return buffer;
    }

    void RenderProcessor::PreProcessToy(const ToyHandle& toy, const std::shared_ptr<Box>& box, FrameBuffer& buffer)
    {
        // Preprocess materials to upload textures and shaders to GPU
        if (box->HasBlockOn<Material>(toy))
        {
            auto& material = box->GetBlockOn<Material>(toy);

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

    void RenderProcessor::ProcessToy(const ToyHandle& toy, const std::shared_ptr<Box>& box, FrameBuffer& buffer)
    {
        // NOTE: Order matters here!!!
        
        // Material block, upload the material data
        if (box->HasBlockOn<Material>(toy))
        {
            auto& material = box->GetBlockOn<Material>(toy);
            buffer.Emplace(DrawCommandType::SetMaterial, material);
        }

        // Model block, upload model data
        if (box->HasBlockOn<Model>(toy))
        {
            const auto& model = box->GetBlockOn<Model>(toy);
            buffer.Emplace(DrawCommandType::SetMaterial, model.GetMaterial());
            buffer.Emplace(DrawCommandType::DrawMesh, model.GetMesh());
        }

        // Transform block, upload the transform data
        if (box->HasBlockOn<Transform>(toy))
        {
            struct VertexUniformBlock
            {
                Tbx::Mat4x4 viewProjectionMatrix;
            };
            const auto& transform = box->GetBlockOn<Transform>(toy);
            const auto transformData = ShaderData(
                "transform",
                Mat4x4::FromTRS(transform.Position, transform.Rotation, transform.Scale),
                ShaderUniformDataType::Mat4);
            buffer.Emplace(DrawCommandType::UploadMaterialData, transformData);
        }
        
        // Mesh block, upload the mesh data
        if (box->HasBlockOn<Mesh>(toy))
        {
            auto& mesh = box->GetBlockOn<Mesh>(toy);
            buffer.Emplace(DrawCommandType::DrawMesh, mesh);
        }

        // Camera block, upload the camera data
        if (box->HasBlockOn<Camera>(toy))
        {
            auto& camera = box->GetBlockOn<Camera>(toy);
            if (box->HasBlockOn<Transform>(toy))
            {
                // Use the transform block's position and rotation
                const auto& cameraTransform = box->GetBlockOn<Transform>(toy);
                const auto viewProjMatrix = Camera::CalculateViewProjectionMatrix(
                    cameraTransform.Position, cameraTransform.Rotation, camera.GetProjectionMatrix());
                const auto shaderData = ShaderData("viewProjection", viewProjMatrix, ShaderUniformDataType::Mat4);
                buffer.Emplace(DrawCommandType::UploadMaterialData, shaderData);
            }
            else
            {
                // No transform block, use default camera position and rotation
                const auto viewProjMatrix = Camera::CalculateViewProjectionMatrix(
                    Constants::Vector3::Zero, Constants::Quaternion::Identity, camera.GetProjectionMatrix());
                const auto shaderData = ShaderData("viewProjection", viewProjMatrix, ShaderUniformDataType::Mat4);
                buffer.Emplace(DrawCommandType::UploadMaterialData, shaderData);
            }
        }
    }
}
