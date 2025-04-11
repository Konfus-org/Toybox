#pragma once
#include "Tbx/App/Render Pipeline/RenderPipeline.h"
#include <Tbx/Core/Rendering/RenderData.h>
#include <Tbx/Core/Rendering/Shader.h>
#include <Tbx/Core/Rendering/Material.h>
#include <Tbx/Core/Rendering/Camera.h>
#include <Tbx/Core/Rendering/Mesh.h>
#include <Tbx/Core/ECS/Toy.h>
#include <Tbx/Core/ECS/Block.h>
#include <Tbx/Core/ECS/Playspace.h>
#include <Tbx/Core/Math/Transform.h>
#include <vector>
#include <memory>

namespace Tbx
{
    /// <summary>
    /// This processes a playspace for rendering.
    /// It takes all the boxes and toys in the playspace and converts them to render data and pushes it to the render pipeline.
    /// </summary>
    class RenderProcessor
    {
    public:
        /// <summary>
        /// Gets playspace ready for rendering.
        /// </summary>
        void PreProcess(const std::shared_ptr<Playspace>& playspace) const
        {
            InitializeBoxes(playspace->GetBoxes());
        }

        /// <summary>
        /// Processes the playspace and sends it to the render pipeline to be rendered.
        /// </summary>
        void Process(const std::shared_ptr<Playspace>& playspace) const
        {
            ProcessBoxes(playspace->GetBoxes());
        }

    private:
        void InitializeBoxes(const std::vector<std::shared_ptr<Box>>& boxes) const
        {
            for (const auto& box : boxes)
            {
                const auto& itemsInBox = box->GetAllItems();
                for (const auto& item : itemsInBox)
                {
                    if (const auto& toy = std::dynamic_pointer_cast<Toy>(item))
                    {
                        InitializeToy(toy);
                    }
                    else if (const auto& boxItem = std::dynamic_pointer_cast<Box>(item))
                    {
                        InitializeBoxes({ boxItem });
                    }
                }
            }
        }

        void InitializeToy(const std::shared_ptr<Toy>& toy) const
        {
            auto material = toy->GetBlock<Material>();
            if (material)
            {
                // Preprocess materials to upload textures and shaders to GPU
                auto renderData = RenderData(RenderCommand::CompileMaterial, material);
                Tbx::RenderPipeline::Push(renderData);

                ////int textureSlot = 0;
                ////for (const auto& texture : material->GetTextures())
                ////{
                ////    const auto& uploadTextureData = 
                ////        Tbx::RenderData(Tbx::RenderCommand::UploadTexture, Tbx::TextureRenderData(texture, textureSlot));
                ////    const auto& shaderData =
                ////        Tbx::ShaderData("textureUniform", textureSlot, Tbx::ShaderDataType::Int);
                ////    const auto& renderShaderData = 
                ////        Tbx::RenderData(Tbx::RenderCommand::UploadShaderData, shaderData);

                ////    Tbx::RenderPipeline::Push(renderShaderData);

                ////    textureSlot++;
                ////}
            }
        }

        void ProcessBoxes(const std::vector<std::shared_ptr<Box>>& boxes) const
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

        void ProcessToy(const std::shared_ptr<Toy>& toy) const
        {
            // Material block, upload the material data
            if (auto material = toy->GetBlock<Material>(); material != nullptr)
            {
                auto renderData = RenderData(RenderCommand::SetMaterial, material);
                RenderPipeline::Push(renderData);
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
                    const auto& renderData = Tbx::RenderData(Tbx::RenderCommand::UploadMaterialShaderData, shaderData);

                    Tbx::RenderPipeline::Push(renderData);
                }
                else
                {
                    // No transform block, use default camera position and rotation
                    const auto& shaderData = Tbx::ShaderData(
                        "viewProjection",
                        Camera::CalculateViewProjectionMatrix(Vector3::Zero(), Quaternion::Identity(), camera->GetProjectionMatrix()),
                        Tbx::ShaderDataType::Mat4);
                    const auto& renderData = Tbx::RenderData(Tbx::RenderCommand::UploadMaterialShaderData, shaderData);

                    Tbx::RenderPipeline::Push(renderData);
                }
            }
            // Transform block, upload the transform data
            if (auto transform = toy->GetBlock<Transform>(); transform != nullptr)
            {
                const auto& shaderData = Tbx::ShaderData(
                    "transform",
                    Tbx::Mat4x4::FromTRS(transform->Position, transform->Rotation, transform->Scale),
                    Tbx::ShaderDataType::Mat4);
                const auto& renderShaderData = Tbx::RenderData(Tbx::RenderCommand::UploadMaterialShaderData, shaderData);

                Tbx::RenderPipeline::Push(renderShaderData);
            }
            // Mesh block, upload the mesh data
            if (auto mesh = toy->GetBlock<Mesh>(); mesh != nullptr)
            {
                auto renderData = RenderData(RenderCommand::RenderMesh, mesh);
                RenderPipeline::Push(renderData);
            }
        }
    };
}