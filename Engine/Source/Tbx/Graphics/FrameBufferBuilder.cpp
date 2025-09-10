#include "Tbx/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/Graphics/FrameBufferBuilder.h"
#include "Tbx/Graphics/Buffers.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Camera.h"
#include "Tbx/Math/Transform.h"
#include "Tbx/Graphics/Frustum.h"
#include "Tbx/Graphics/Sphere.h"

namespace Tbx
{
    FrameBuffer FrameBufferBuilder::BuildUploadBuffer(WorldView<MaterialInstance, Mesh, Model> view)
    {
        FrameBuffer buffer = {};

        for (const auto& toy : view)
        {
            AddToyUploadCommandsToBuffer(toy, buffer);
        }

        return buffer;
    }

    FrameBuffer FrameBufferBuilder::BuildRenderBuffer(FullWorldView view)
    {
        // Build view frustums from cameras
        auto frustums = std::vector<Frustum>();
        for (const auto& toy : view)
        {
            if (!toy->HasBlock<Camera>())
            {
                continue;
            }

            auto& camera = toy->GetBlock<Camera>();

            Vector3 camPos = Constants::Vector3::Zero;
            Quaternion camRot = Constants::Quaternion::Identity;
            if (toy->HasBlock<Transform>())
            {
                const auto& camTransform = toy->GetBlock<Transform>();
                camPos = camTransform.Position;
                camRot = camTransform.Rotation;
            }

            // Extract the planes that make up the camera frustum
            frustums.push_back(Camera::CalculateFrustum(camPos, camRot, camera.GetProjectionMatrix()));
        }

        // If no camera is available, we have no view frustum and nothing to draw
        if (frustums.empty()) return {};

        // Iterate through toys and skip those completely outside the view frustum
        FrameBuffer buffer = {};
        for (const auto& toy : view)
        {
            if (!toy->HasBlock<Camera>())
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

    void FrameBufferBuilder::AddToyUploadCommandsToBuffer(const std::shared_ptr<Toy>& toy, FrameBuffer& buffer)
    {
        // Preprocess materials to upload textures and shaders to GPU
        if (toy->HasBlock<MaterialInstance>())
        {
            const auto& material = toy->GetBlock<MaterialInstance>();

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
        if (toy->HasBlock<Model>())
        {
            const auto& model = toy->GetBlock<Model>();
            buffer.Emplace(DrawCommandType::UploadMaterial, model.GetMaterial());
            buffer.Emplace(DrawCommandType::UploadMesh, model.GetMesh());
        }

        // Preprocess meshes to upload the mesh data
        if (toy->HasBlock<Mesh>())
        {
            const auto& mesh = toy->GetBlock<Mesh>();
            buffer.Emplace(DrawCommandType::UploadMesh, mesh);
        }
    }

    void FrameBufferBuilder::AddToyRenderCommandsToBuffer(const std::shared_ptr<Toy>& toy, FrameBuffer& buffer)
    {
        // NOTE: Order matters here!!!
        
        // Material block, should be a material instance
        if (toy->HasBlock<Material>())
        {
            TBX_ASSERT(false, "A toy shouldn't use a material directly! Toys should use material instances!");
            return;
        }
        
        // Material block, upload the material data
        if (toy->HasBlock<MaterialInstance>())
        {
            const auto& material = toy->GetBlock<MaterialInstance>();
            buffer.Emplace(DrawCommandType::SetMaterial, material);
        }

        // Transform block, upload the transform data
        if (toy->HasBlock<Transform>())
        {
            const auto& transform = toy->GetBlock<Transform>();
            const auto transformMatrix = Mat4x4::FromTRS(transform.Position, transform.Rotation, transform.Scale);
            buffer.Emplace(DrawCommandType::SetUniform, ShaderUniform("TransformUniform", transformMatrix, ShaderUniformDataType::Mat4));
        }

        // Camera block, upload the camera data
        if (toy->HasBlock<Camera>())
        {
            auto& camera = toy->GetBlock<Camera>();

            if (toy->HasBlock<Transform>())
            {
                // Use the transform block's position and rotation
                const auto& cameraTransform = toy->GetBlock<Transform>();
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
        if (toy->HasBlock<Model>())
        {
            const auto& model = toy->GetBlock<Model>();
            buffer.Emplace(DrawCommandType::SetMaterial, model.GetMaterial());
            buffer.Emplace(DrawCommandType::DrawMesh, model.GetMesh());
        }
        
        // Mesh block, upload the mesh data
        if (toy->HasBlock<Mesh>())
        {
            const auto& mesh = toy->GetBlock<Mesh>();
            buffer.Emplace(DrawCommandType::DrawMesh, mesh);
        }
    }
}
