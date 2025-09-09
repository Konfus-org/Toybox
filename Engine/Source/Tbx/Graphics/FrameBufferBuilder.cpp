#include "Tbx/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/Graphics/FrameBufferBuilder.h"
#include "Tbx/Graphics/Buffers.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Material.h"
#include "Tbx/Graphics/Camera.h"
#include "Tbx/Graphics/Mesh.h"
#include "Tbx/Graphics/Model.h"
#include "Tbx/Math/Transform.h"
#include "Tbx/Graphics/Frustum.h"
#include "Tbx/Graphics/BoundingSphere.h"

namespace Tbx
{
    FrameBuffer FrameBufferBuilder::BuildUploadBuffer(const std::vector<std::shared_ptr<Box>>& boxes)
    {
        FrameBuffer buffer = {};

        for (const auto& box : boxes)
        {
            // We only need to pre-process materials to upload them to the GPU
            for (const auto& toy : BoxView<MaterialInstance, Mesh, Model>(box))
            {
                AddToyUploadCommandsToBuffer(toy, box, buffer);
            }
        }

        return buffer;
    }

    FrameBuffer FrameBufferBuilder::BuildRenderBuffer(const std::vector<std::shared_ptr<Box>>& boxes)
    {
        // Build a view frustum from the first available camera
        bool boxesContainsCamera = false;
        auto frustums = std::vector<Frustum>();

        for (const auto& box : boxes)
        {
            for (const auto& camToy : BoxView<Camera>(box))
            {
                auto& camera = box->GetBlockOn<Camera>(camToy);

                Vector3 camPos = Constants::Vector3::Zero;
                Quaternion camRot = Constants::Quaternion::Identity;
                if (box->HasBlockOn<Transform>(camToy))
                {
                    const auto& camTransform = box->GetBlockOn<Transform>(camToy);
                    camPos = camTransform.Position;
                    camRot = camTransform.Rotation;
                }

                // Extract the planes that make up the camera frustum
                frustums.push_back(Camera::CalculateFrustum(camPos, camRot, camera.GetProjectionMatrix()));
                boxesContainsCamera = true;
                break;
            }
        }

        // Start with an empty command buffer; if no camera is found this will be returned as-is
        FrameBuffer buffer = {};

        // If no camera is available, we have no view frustum and nothing to draw
        if (!boxesContainsCamera) return buffer;

        // Iterate through toys and skip those completely outside the view frustum
        for (const auto& box : boxes)
        {
            for (const auto& toy : BoxView(box))
            {
                if (!box->HasBlockOn<Camera>(toy))
                {
                    const auto sphere = BoundingSphere(toy, box);

                    // Check if the sphere is in at least one of our frustums
                    bool inFrustum = false;
                    for (const auto& frustum : frustums)
                    {
                        if (frustum.Intersects(sphere))
                        {
                            inFrustum = true;
                            break;
                        }
                    }

                    // Skip toy if it's outside the view frustum
                    if (!inFrustum) continue;
                }
                AddToyRenderCommandsToBuffer(toy, box, buffer);
            }
        }

        return buffer;
    }

    void FrameBufferBuilder::AddToyUploadCommandsToBuffer(const ToyHandle& toy, const std::shared_ptr<Box>& box, FrameBuffer& buffer)
    {
        // Preprocess materials to upload textures and shaders to GPU
        if (box->HasBlockOn<MaterialInstance>(toy))
        {
            const auto& material = box->GetBlockOn<MaterialInstance>(toy);

            // Check if we already added this material
            auto newMaterialToUpload = true;
            const auto& existingUploadMatCmds = buffer.GetCommands();
            for (const auto& cmd : existingUploadMatCmds)
            {
                if (cmd.GetType() != DrawCommandType::UploadMaterial) continue;

                const auto& materialToUpload = std::any_cast<const MaterialInstance&>(cmd.GetPayload());
                if (materialToUpload.GetId() == material)
                {
                    newMaterialToUpload = false;
                    break;
                }
            }
            if (newMaterialToUpload)
            {
                // New material, add to buffer
                buffer.Emplace(DrawCommandType::UploadMaterial, material);
            }
        }

        // Preprocess models to upload mesh and its material data (shaders and textures) to GPU
        if (box->HasBlockOn<Model>(toy))
        {
            const auto& model = box->GetBlockOn<Model>(toy);
            buffer.Emplace(DrawCommandType::UploadMaterial, model.GetMaterial());
            buffer.Emplace(DrawCommandType::UploadMesh, model.GetMesh());
        }

        // Preprocess meshes to upload the mesh data
        if (box->HasBlockOn<Mesh>(toy))
        {
            const auto& mesh = box->GetBlockOn<Mesh>(toy);
            buffer.Emplace(DrawCommandType::UploadMesh, mesh);
        }
    }

    void FrameBufferBuilder::AddToyRenderCommandsToBuffer(const ToyHandle& toy, const std::shared_ptr<Box>& box, FrameBuffer& buffer)
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
            buffer.Emplace(DrawCommandType::SetUniform, ShaderUniform("TransformUniform", transformMatrix, ShaderUniformDataType::Mat4));
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
                buffer.Emplace(DrawCommandType::SetUniform, ShaderUniform("ViewProjectionUniform", viewProjMatrix, ShaderUniformDataType::Mat4));
            }
            else
            {
                // No transform block, use default camera position and rotation
                const auto viewProjMatrix = Camera::CalculateViewProjectionMatrix(
                    Constants::Vector3::Zero, Constants::Quaternion::Identity, camera.GetProjectionMatrix());
                buffer.Emplace(DrawCommandType::SetUniform, ShaderUniform("ViewProjectionUniform", viewProjMatrix, ShaderUniformDataType::Mat4));
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
