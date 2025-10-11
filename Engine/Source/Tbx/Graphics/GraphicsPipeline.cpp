#include "Tbx/PCH.h"
#include "Tbx/Graphics/GraphicsPipeline.h"
#include "Tbx/Debug/Asserts.h"
#include "Tbx/Graphics/Model.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Texture.h"
#include "Tbx/Graphics/Camera.h"
#include "Tbx/Stages/Views.h"
#include "Tbx/Math/Mat4x4.h"
#include "Tbx/Math/Transform.h"
#include "Tbx/Graphics/Frustum.h"
#include "Tbx/Graphics/Sphere.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Stages/Stage.h"
#include <string>
#include <unordered_map>
#include <utility>

namespace Tbx
{
    const std::string TRANSFORM_UNIFORM_NAME = "TransformUniform";
    const std::string VIEW_PROJECTION_UNIFORM_NAME = "ViewProjectionUniform";

    GraphicsPipeline::GraphicsPipeline(std::vector<RenderPass> passes)
    {
        RenderPasses = passes;
    }

    void GraphicsPipeline::Render(
        GraphicsRenderer& renderer,
        const GraphicsDisplay& display, 
        const std::vector<Ref<Stage>>& stages,
        const RgbaColor& clearColor)
    {
        TBX_ASSERT(&renderer, "GraphicsPipeline: No active renderer has been set for the current graphics API.");
        TBX_ASSERT(display.Surface && display.Context, "GraphicsPipeline: Display is missing a surface or context.");

        auto aspectRatio = display.Surface->GetSize().GetAspectRatio();
        auto viewport = Viewport({0, 0}, display.Surface->GetSize());
        display.Context->MakeCurrent();
        renderer.Backend->SetContext(display.Context);
        renderer.Backend->BeginDraw(clearColor, viewport);

        for (const auto& stage : stages)
        {
            auto renderData = PrepareStageForRendering(renderer, FullStageView(stage->GetRoot()), aspectRatio);

            for (uint32 passIndex = 0; passIndex < RenderPasses.size(); ++passIndex)
            {
                RenderStage(passIndex, renderer, renderData);
            }
        }

        display.Context->Present();
        renderer.Backend->EndDraw();
    }

    void GraphicsPipeline::RenderStage(uint32 passIndex, Tbx::GraphicsRenderer& renderer, Tbx::StageRenderData& renderData)
    {
        const auto& pass = RenderPasses[passIndex];
        renderer.Backend->EnableDepthTesting(pass.DepthTestEnabled);

        const auto& buckets = renderData.PassBuckets[passIndex];
        for (const auto& [shaderProgramId, bucket] : buckets)
        {
            const auto shaderResourceIt = renderer.Cache.ShaderPrograms.find(shaderProgramId);
            if (shaderResourceIt == renderer.Cache.ShaderPrograms.end() || !shaderResourceIt->second)
            {
                continue;
            }

            const auto& shaderResource = shaderResourceIt->second;
            UseGraphicsResourceScope shaderScope(shaderResource);

            for (const auto& camera : renderData.Cameras)
            {
                ShaderUniform viewProjectionUniform = {};
                viewProjectionUniform.Name = VIEW_PROJECTION_UNIFORM_NAME;
                viewProjectionUniform.Data = camera.ViewProjection;
                shaderResource->Upload(viewProjectionUniform);

                RenderCameraView(bucket, camera, shaderResource, renderer);
            }
        }
    }

    void GraphicsPipeline::RenderCameraView(const Tbx::RenderBucket& bucket, const Tbx::CameraData& camera, const Tbx::Ref<Tbx::ShaderProgramResource>& shaderResource, Tbx::GraphicsRenderer& renderer)
    {
        for (const auto& entityPtr : bucket)
        {
            if (ShouldCull(entityPtr, camera.Frustum))
            {
                continue;
            }

            const auto& entity = *entityPtr;

            Mat4x4 transformMatrix = Mat4x4::Identity;
            if (entity.Blocks.Contains<Transform>())
            {
                const auto& transform = entity.Blocks.Get<Transform>();
                transformMatrix = Mat4x4::FromTRS(transform.Position, transform.Rotation, transform.Scale);
            }

            ShaderUniform transformUniform = {};
            transformUniform.Name = TRANSFORM_UNIFORM_NAME;
            transformUniform.Data = transformMatrix;
            shaderResource->Upload(transformUniform);

            std::vector<UseGraphicsResourceScope> textureScopes = {};
            const auto& material = entity.Blocks.Get<Material>();
            textureScopes.reserve(material.Textures.size());
            for (size_t textureIndex = 0; textureIndex < material.Textures.size(); ++textureIndex)
            {
                const auto& texture = material.Textures[textureIndex];
                if (!texture)
                {
                    continue;
                }

                const auto textureResourceIt = renderer.Cache.Textures.find(texture->Id);
                if (textureResourceIt == renderer.Cache.Textures.end() || !textureResourceIt->second)
                {
                    continue;
                }

                const auto& textureResource = textureResourceIt->second;
                textureResource->SetSlot(static_cast<uint32>(textureIndex));
                textureScopes.emplace_back(textureResource);
            }

            const auto& mesh = entity.Blocks.Get<Mesh>();
            const auto meshResourceIt = renderer.Cache.Meshes.find(mesh.Id);
            if (meshResourceIt == renderer.Cache.Meshes.end() || !meshResourceIt->second)
            {
                continue;
            }

            UseGraphicsResourceScope meshScope(meshResourceIt->second);
            meshResourceIt->second->Draw();
        }
    }

    StageRenderData GraphicsPipeline::PrepareStageForRendering(
        GraphicsRenderer& renderer,
        const FullStageView& stageView,
        float aspectRatio)
    {
        StageRenderData renderData = {};
        renderData.PassBuckets.resize(RenderPasses.size());

        for (const auto& toy : stageView)
        {
            auto& toyBlocks = toy->Blocks;
            if (toyBlocks.Contains<Camera>())
            {
                auto& camera = toyBlocks.Get<Camera>();
                TBX_ASSERT(aspectRatio > 0.0f, "GraphicsPipeline: aspect ratio must be positive.");
                if (aspectRatio > 0.0f)
                {
                    camera.SetAspect(aspectRatio);
                }

                Vector3 camPos = Vector3::Zero;
                Quaternion camRot = Quaternion::Identity;
                if (toyBlocks.Contains<Transform>())
                {
                    const auto& camTransform = toyBlocks.Get<Transform>();
                    camPos = camTransform.Position;
                    camRot = camTransform.Rotation;
                }

                renderData.Cameras.emplace_back(
                    Camera::CalculateViewProjectionMatrix(camPos, camRot, camera.GetProjectionMatrix()),
                    Camera::CalculateFrustum(camPos, camRot, camera.GetProjectionMatrix()));
            }
            if (toyBlocks.Contains<Model>() && toyBlocks.Contains<Material>() ||
                toyBlocks.Contains<Model>() && toyBlocks.Contains<Mesh>())
            {
                TBX_ASSERT(false, "GraphicsPipeline: You can have a mesh and material, or a model. Not both!");
            }
            else if (toyBlocks.Contains<Model>())
            {
                const auto& model = toyBlocks.Get<Model>();
                CacheMaterial(renderer, model.Material);
                CacheMesh(renderer, model.Mesh);
                const size_t passIndex = ResolveRenderPassIndex(model.Material);
                auto& passBuckets = renderData.PassBuckets[passIndex];
                passBuckets[model.Material.ShaderProgram.Id].push_back(toy);
            }
            else if (toyBlocks.Contains<Material>() && toyBlocks.Contains<Mesh>())
            {
                const auto& mesh = toyBlocks.Get<Mesh>();
                const auto& material = toyBlocks.Get<Material>();
                CacheMaterial(renderer, material);
                CacheMesh(renderer, mesh);
                const size_t passIndex = ResolveRenderPassIndex(material);
                auto& passBuckets = renderData.PassBuckets[passIndex];
                passBuckets[material.ShaderProgram.Id].push_back(toy);
            }
        }

        return renderData;
    }

    bool GraphicsPipeline::ShouldCull(const Ref<Toy>& toy, const Frustum& frustum)
    {
        if (!toy)
        {
            return true;
        }

        if (toy->Blocks.Contains<Camera>())
        {
            return false;
        }

        if (!toy->Blocks.Contains<Mesh>() ||
            !toy->Blocks.Contains<Material>())
        {
            return false;
        }

        if (frustum.Intersects(BoundingSphere(toy)))
        {
            return false;
        }

        return true;
    }

    void GraphicsPipeline::CacheShaders(GraphicsRenderer& renderer, const ShaderProgram& shaders)
    {
        auto& cache = renderer.Cache;

        std::vector<Ref<ShaderResource>> shaderResources = {};
        shaderResources.reserve(shaders.Shaders.size());

        for (const auto& shader : shaders.Shaders)
        {
            if (!shader)
            {
                continue;
            }

            auto resourceIt = cache.Shaders.find(shader->Id);
            if (resourceIt == cache.Shaders.end() || !resourceIt->second)
            {
                auto resource = renderer.Backend->CompileShader(*shader);
                TBX_ASSERT(resource, "GraphicsPipeline: Unable to compile shader for active graphics api.");
                if (!resource)
                {
                    continue;
                }

                resourceIt = cache.Shaders.emplace(shader->Id, resource).first;
            }

            shaderResources.push_back(resourceIt->second);
        }

        if (cache.ShaderPrograms.contains(shaders.Id))
        {
            return;
        }

        auto shaderProgram = renderer.Backend->CreateShaderProgram(shaderResources);
        TBX_ASSERT(shaderProgram, "GraphicsPipeline: Unable to cache shader programs for active graphics api.");
        if (!shaderProgram)
        {
            return;
        }

        cache.ShaderPrograms.emplace(shaders.Id, shaderProgram);
    }

    void GraphicsPipeline::CacheMaterial(GraphicsRenderer& renderer, const Material& material)
    {
        CacheShaders(renderer, material.ShaderProgram);

        auto& cache = renderer.Cache;

        for (const auto& texture : material.Textures)
        {
            if (!texture)
            {
                continue;
            }

            if (cache.Textures.contains(texture->Id))
            {
                continue;
            }

            auto resource = renderer.Backend->UploadTexture(*texture);
            TBX_ASSERT(resource, "GraphicsPipeline: Unable to cache textures for active graphics api.");
            if (!resource)
            {
                continue;
            }

            cache.Textures.emplace(texture->Id, resource);
        }
    }

    void GraphicsPipeline::CacheMesh(GraphicsRenderer& renderer, const Mesh& mesh)
    {
        if (renderer.Cache.Meshes.contains(mesh.Id))
        {
            return;
        }

        auto resource = renderer.Backend->UploadMesh(mesh);
        TBX_ASSERT(resource, "GraphicsPipeline: Unable to cache meshes for active graphics api.");
        if (!resource)
        {
            return;
        }

        renderer.Cache.Meshes.emplace(mesh.Id, resource);
    }

    size_t GraphicsPipeline::ResolveRenderPassIndex(const Material& material) const
    {
        TBX_ASSERT(!RenderPasses.empty(), "GraphicsPipeline: Render passes must be configured before drawing.");
        if (RenderPasses.empty())
        {
            return 0;
        }

        size_t fallbackIndex = 0;
        bool hasFallback = false;
        for (size_t i = 0; i < RenderPasses.size(); ++i)
        {
            const auto& pass = RenderPasses[i];
            if (!pass.Filter)
            {
                if (!hasFallback)
                {
                    fallbackIndex = i;
                    hasFallback = true;
                }
                continue;
            }

            if (pass.Filter(material))
            {
                return i;
            }
        }

        return hasFallback ? fallbackIndex : 0;
    }
}
