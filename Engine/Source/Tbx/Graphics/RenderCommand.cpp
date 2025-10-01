#include "Tbx/PCH.h"
#include "Tbx/Graphics/RenderCommands.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Camera.h"
#include "Tbx/Math/Transform.h"
#include "Tbx/Graphics/Frustum.h"
#include "Tbx/Graphics/Sphere.h"
#include "Tbx/Debug/Debugging.h"

namespace Tbx
{
    RenderCommandBuffer RenderCommandBufferBuilder::BuildUploadBuffer(StageView<MaterialInstance, Mesh, Model> view)
    {
        RenderCommandBuffer buffer = {};

        for (const auto& toy : view)
        {
            AddToyUploadCommandsToBuffer(toy, buffer);
        }

        return buffer;
    }

    RenderCommandBuffer RenderCommandBufferBuilder::BuildRenderBuffer(FullStageViewView view, float aspectRatio)
    {
        // Build view frustums from cameras
        auto frustums = std::vector<Frustum>();
        for (const auto& toy : view)
        {
            if (!toy->Blocks.Contains<Camera>())
            {
                continue;
            }

            auto& camera = toy->Blocks.Get<Camera>();

            TBX_ASSERT(aspectRatio > 0.0f, "RenderCommandBufferBuilder: aspect ratio must be positive.");
            if (aspectRatio > 0.0f)
            {
                camera.SetAspect(aspectRatio);
            }

            Vector3 camPos = Vector3::Zero;
            Quaternion camRot = Quaternion::Identity;
            if (toy->Blocks.Contains<Transform>())
            {
                const auto& camTransform = toy->Blocks.Get<Transform>();
                camPos = camTransform.Position;
                camRot = camTransform.Rotation;
            }

            // Extract the planes that make up the camera frustum
            frustums.push_back(Camera::CalculateFrustum(camPos, camRot, camera.GetProjectionMatrix()));
        }

        // If no camera is available, we have no view frustum and nothing to draw
        if (frustums.empty()) return {};

        // Iterate through toys and skip those completely outside the view frustum
        RenderCommandBuffer buffer = {};
        for (const auto& toy : view)
        {
            if (!toy->Blocks.Contains<Camera>())
            {
                const auto sphere = BoundingSphere(toy);

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
            AddToyRenderCommandsToBuffer(toy, buffer);
        }

        return buffer;
    }

    void RenderCommandBufferBuilder::AddToyUploadCommandsToBuffer(const Ref<Toy>& toy, RenderCommandBuffer& buffer)
    {
        // Preprocess materials to upload textures and shaders to GPU
        if (toy->Blocks.Contains<MaterialInstance>())
        {
            const auto& material = toy->Blocks.Get<MaterialInstance>();

            // Check if we already added this material
            auto newMaterialToUpload = true;
            const auto& existingUploadMatCmds = buffer.Commands;
            for (const auto& cmd : existingUploadMatCmds)
            {
                if (cmd.Type != RenderCommandType::UploadMaterial) continue;

                const auto& materialToUpload = std::any_cast<const MaterialInstance&>(cmd.Payload);
                if (materialToUpload.Id == material.Id)
                {
                    newMaterialToUpload = false;
                    break;
                }
            }
            if (newMaterialToUpload)
            {
                // New material, add to buffer
                buffer.Commands.emplace_back(RenderCommandType::UploadMaterial, material);
            }
        }

        // Preprocess models to upload mesh and its material data (shaders and textures) to GPU
        if (toy->Blocks.Contains<Model>())
        {
            const auto& model = toy->Blocks.Get<Model>();
            buffer.Commands.emplace_back(RenderCommandType::UploadMaterial, model.GetMaterial());
            buffer.Commands.emplace_back(RenderCommandType::UploadMesh, model.GetMesh());
        }

        // Preprocess meshes to upload the mesh data
        if (toy->Blocks.Contains<Mesh>())
        {
            const auto& mesh = toy->Blocks.Get<Mesh>();
            buffer.Commands.emplace_back(RenderCommandType::UploadMesh, mesh);
        }
    }

    void RenderCommandBufferBuilder::AddToyRenderCommandsToBuffer(const Ref<Toy>& toy, RenderCommandBuffer& buffer)
    {
        // NOTE: Order matters here!!!
        
        // Material block, should be a material instance
        if (toy->Blocks.Contains<Material>())
        {
            TBX_ASSERT(false, "RenderCommandBufferBuilder: A toy shouldn't use a material directly! Toys should use material instances!");
            return;
        }
        
        // Material block, upload the material data
        if (toy->Blocks.Contains<MaterialInstance>())
        {
            const auto& material = toy->Blocks.Get<MaterialInstance>();
            buffer.Commands.emplace_back(RenderCommandType::SetMaterial, material);
        }

        // Camera block, upload the camera data
        if (toy->Blocks.Contains<Camera>())
        {
            const auto& camera = toy->Blocks.Get<Camera>();

            if (toy->Blocks.Contains<Transform>())
            {
                // Use the transform block's position and rotation
                const auto& cameraTransform = toy->Blocks.Get<Transform>();
                const auto viewProjMatrix = Camera::CalculateViewProjectionMatrix(cameraTransform.Position, cameraTransform.Rotation, camera.GetProjectionMatrix());
                buffer.Commands.emplace_back(RenderCommandType::SetUniform, ShaderUniform("ViewProjectionUniform", viewProjMatrix));
            }
            else
            {
                // No transform block, use default camera position and rotation
                const auto viewProjMatrix = Camera::CalculateViewProjectionMatrix(
                    Vector3::Zero, Quaternion::Identity, camera.GetProjectionMatrix());
                buffer.Commands.emplace_back(RenderCommandType::SetUniform, ShaderUniform("ViewProjectionUniform", viewProjMatrix));
            }
        }
        // Transform block, upload the transform data
        else if (toy->Blocks.Contains<Transform>())
        {
            const auto& transform = toy->Blocks.Get<Transform>();
            const auto transformMatrix = Mat4x4::FromTRS(transform.Position, transform.Rotation, transform.Scale);
            buffer.Commands.emplace_back(RenderCommandType::SetUniform, ShaderUniform("TransformUniform", transformMatrix));
        }

        // Model block, upload model data
        if (toy->Blocks.Contains<Model>())
        {
            const auto& model = toy->Blocks.Get<Model>();
            buffer.Commands.emplace_back(RenderCommandType::SetMaterial, model.GetMaterial());
            buffer.Commands.emplace_back(RenderCommandType::DrawMesh, model.GetMesh());
        }
        
        // Mesh block, upload the mesh data
        if (toy->Blocks.Contains<Mesh>())
        {
            const auto& mesh = toy->Blocks.Get<Mesh>();
            buffer.Commands.emplace_back(RenderCommandType::DrawMesh, mesh);
        }
    }
}
