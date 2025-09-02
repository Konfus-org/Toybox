#include "Tbx/PCH.h"
#include "Tbx/App/App.h"
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
            for (const auto& toy : BoxView<MaterialInstance, Mesh, Model>(box))
            {
                PreProcessToy(toy, box, buffer);
            }
        }

        return buffer;
    }

    FrameBuffer RenderProcessor::Process(const std::vector<std::shared_ptr<Box>>& boxes)
    {
        FrameBuffer buffer = {};

        auto hasAnyCamera = false;
        for (const auto& box : boxes)
        {
            for (const auto& cam : BoxView<Camera>(box))
            {
                hasAnyCamera = true;
                break;
            }
        }
        if (hasAnyCamera)
        {
            for (const auto& box : boxes)
            {
                for (const auto& toy : BoxView(box))
                {
                    ProcessToy(toy, box, buffer);
                }
            }
        }

        return buffer;
    }

    void RenderProcessor::PreProcessToy(const ToyHandle& toy, const std::shared_ptr<Box>& box, FrameBuffer& buffer)
    {
        // Preprocess materials to upload textures and shaders to GPU
        if (box->HasBlockOn<MaterialInstance>(toy))
        {
            const auto& material = box->GetBlockOn<MaterialInstance>(toy);

            // Check if we already added this material
            const auto& existingCompileMatCmds = buffer.GetCommands();
            for (const auto& cmd : existingCompileMatCmds)
            {
                if (cmd.GetType() != DrawCommandType::CompileMaterial) continue;

                const auto& materialToCompile = std::any_cast<const MaterialInstance&>(cmd.GetPayload());
                if (materialToCompile.GetId() == material)
                {
                    return;
                }
            }

            // New material, add to buffer
            buffer.Emplace(DrawCommandType::CompileMaterial, material);
        }

        // Preprocess models to upload mesh and its material data (shaders and textures) to GPU
        if (box->HasBlockOn<Model>(toy))
        {
            const auto& model = box->GetBlockOn<Model>(toy);
            buffer.Emplace(DrawCommandType::CompileMaterial, model.GetMaterial());
            buffer.Emplace(DrawCommandType::UploadMesh, model.GetMesh());
        }

        // Preprocess meshes to upload the mesh data
        if (box->HasBlockOn<Mesh>(toy))
        {
            const auto& mesh = box->GetBlockOn<Mesh>(toy);
            buffer.Emplace(DrawCommandType::UploadMesh, mesh);
        }
    }

    void RenderProcessor::ProcessToy(const ToyHandle& toy, const std::shared_ptr<Box>& box, FrameBuffer& buffer)
    {
        // NOTE: Order matters here!!!
        
        // Material block, should be a material instance
        if (box->HasBlockOn<Material>(toy))
        {
            TBX_ASSERT(false, "A toy shouldn't use a material directly! Toys should use material instances!");
            return;
        }
        
        // Material block, upload the material data
        if (box->HasBlockOn<MaterialInstance>(toy))
        {
            const auto& material = box->GetBlockOn<MaterialInstance>(toy);
            buffer.Emplace(DrawCommandType::SetMaterial, material);
        }

        // Transform block, upload the transform data
        if (box->HasBlockOn<Transform>(toy))
        {
            const auto& transform = box->GetBlockOn<Transform>(toy);
            const auto transformMatrix = Mat4x4::FromTRS(transform.Position, transform.Rotation, transform.Scale);
            buffer.Emplace(DrawCommandType::UploadUniform, ShaderUniform("TransformUniform", transformMatrix, ShaderUniformDataType::Mat4));
        }

        // Camera block, upload the camera data
        if (box->HasBlockOn<Camera>(toy))
        {
            auto& camera = box->GetBlockOn<Camera>(toy);

            // TODO: process this somewhere else!
            const auto& mainWindow = App::GetInstance()->GetMainWindow();
            const auto mainWindowSize = mainWindow.lock()->GetSize();
            const auto aspectRatio = mainWindowSize.GetAspectRatio();
            camera.SetAspect(aspectRatio);

            if (box->HasBlockOn<Transform>(toy))
            {
                // Use the transform block's position and rotation
                const auto& cameraTransform = box->GetBlockOn<Transform>(toy);
                const auto viewProjMatrix = Camera::CalculateViewProjectionMatrix(cameraTransform.Position, cameraTransform.Rotation, camera.GetProjectionMatrix());
                buffer.Emplace(DrawCommandType::UploadUniform, ShaderUniform("ViewProjectionUniform", viewProjMatrix, ShaderUniformDataType::Mat4));
            }
            else
            {
                // No transform block, use default camera position and rotation
                const auto viewProjMatrix = Camera::CalculateViewProjectionMatrix(
                    Constants::Vector3::Zero, Constants::Quaternion::Identity, camera.GetProjectionMatrix());
                buffer.Emplace(DrawCommandType::UploadUniform, ShaderUniform("ViewProjectionUniform", viewProjMatrix, ShaderUniformDataType::Mat4));
            }
        }

        // Model block, upload model data
        if (box->HasBlockOn<Model>(toy))
        {
            const auto& model = box->GetBlockOn<Model>(toy);
            buffer.Emplace(DrawCommandType::SetMaterial, model.GetMaterial());
            buffer.Emplace(DrawCommandType::DrawMesh, model.GetMesh());
        }
        
        // Mesh block, upload the mesh data
        if (box->HasBlockOn<Mesh>(toy))
        {
            const auto& mesh = box->GetBlockOn<Mesh>(toy);
            buffer.Emplace(DrawCommandType::DrawMesh, mesh);
        }
    }
}
