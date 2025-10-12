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

    StageDrawData GraphicsPipeline::Prepare(
        GraphicsRenderer& renderer,
        const FullStageView& stageView,
        float aspectRatio)
    {
        StageDrawData renderData = {};
        renderData.PassBuckets.resize(RenderPasses.size());

        std::unordered_map<Uid, const Mesh*> meshRegistry = {};
        std::unordered_map<Uid, const Material*> materialRegistry = {};
        std::unordered_map<Uid, const Model*> modelRegistry = {};

        meshRegistry.emplace(Mesh::Quad.Id, &Mesh::Quad);
        meshRegistry.emplace(Mesh::Triangle.Id, &Mesh::Triangle);

        for (const auto& toy : stageView)
        {
            if (!toy)
            {
                continue;
            }

            Ref<Mesh> meshRef;
            if (toy->TryGet(meshRef) && meshRef)
            {
                meshRegistry[meshRef->Id] = meshRef.get();
            }

            Ref<Material> materialRef;
            if (toy->TryGet(materialRef) && materialRef)
            {
                materialRegistry[materialRef->Id] = materialRef.get();
            }

            Ref<Model> modelRef;
            if (toy->TryGet(modelRef) && modelRef)
            {
                modelRegistry[modelRef->Id] = modelRef.get();
                meshRegistry[modelRef->Poly.Id] = &modelRef->Poly;
                materialRegistry[modelRef->Mat.Id] = &modelRef->Mat;
            }
        }

        for (const auto& toy : stageView)
        {
            if (!toy)
            {
                continue;
            }

            if (toy->Has<Camera>())
            {
                auto camera = toy->Get<Camera>();
                TBX_ASSERT(aspectRatio > 0.0f, "GraphicsPipeline: aspect ratio must be positive.");
                if (aspectRatio > 0.0f)
                {
                    camera->SetAspect(aspectRatio);
                }

                Vector3 camPos = Vector3::Zero;
                Quaternion camRot = Quaternion::Identity;
                if (toy->Has<Transform>())
                {
                    const auto camTransform = toy->Get<Transform>();
                    camPos = camTransform->Position;
                    camRot = camTransform->Rotation;
                }

                renderData.Cameras.emplace_back(
                    Camera::CalculateViewProjectionMatrix(camPos, camRot, camera->GetProjectionMatrix()),
                    Camera::CalculateFrustum(camPos, camRot, camera->GetProjectionMatrix()));
            }
            const bool hasModel = toy->Has<Model>();
            const bool hasModelInstance = toy->Has<ModelInstance>();
            const bool hasMesh = toy->Has<Mesh>();
            const bool hasMeshInstance = toy->Has<MeshInstance>();
            const bool hasMaterial = toy->Has<Material>();
            const bool hasMaterialInstance = toy->Has<MaterialInstance>();

            if ((hasModel && (hasModelInstance || hasMesh || hasMeshInstance || hasMaterial || hasMaterialInstance)) ||
                (hasModelInstance && (hasMesh || hasMeshInstance || hasMaterial || hasMaterialInstance)) ||
                (hasMesh && hasMeshInstance) ||
                (hasMaterial && hasMaterialInstance))
            {
                TBX_ASSERT(false, "GraphicsPipeline: Conflicting model, mesh, or material blocks on toy.");
                continue;
            }

            const Model* resolvedModel = nullptr;
            if (hasModel)
            {
                const auto model = toy->Get<Model>();
                resolvedModel = model.get();
            }
            else if (hasModelInstance)
            {
                Ref<ModelInstance> modelInstance;
                if (toy->TryGet(modelInstance) && modelInstance)
                {
                    auto modelIt = modelRegistry.find(modelInstance->ModelId);
                    if (modelIt != modelRegistry.end())
                    {
                        resolvedModel = modelIt->second;
                    }
                }
            }

            const Material* resolvedMaterial = nullptr;
            if (resolvedModel)
            {
                resolvedMaterial = &resolvedModel->Mat;
            }
            else if (hasMaterial)
            {
                const auto material = toy->Get<Material>();
                resolvedMaterial = material.get();
            }
            else if (hasMaterialInstance)
            {
                Ref<MaterialInstance> materialInstance;
                if (toy->TryGet(materialInstance) && materialInstance)
                {
                    auto materialIt = materialRegistry.find(materialInstance->MaterialId);
                    if (materialIt != materialRegistry.end())
                    {
                        resolvedMaterial = materialIt->second;
                    }
                }
            }

            const Mesh* resolvedMesh = nullptr;
            if (resolvedModel)
            {
                resolvedMesh = &resolvedModel->Poly;
            }
            else if (hasMesh)
            {
                const auto mesh = toy->Get<Mesh>();
                resolvedMesh = mesh.get();
            }
            else if (hasMeshInstance)
            {
                Ref<MeshInstance> meshInstance;
                if (toy->TryGet(meshInstance) && meshInstance)
                {
                    auto meshIt = meshRegistry.find(meshInstance->MeshId);
                    if (meshIt != meshRegistry.end())
                    {
                        resolvedMesh = meshIt->second;
                    }
                }
            }

            if (!resolvedMaterial || !resolvedMesh)
            {
                continue;
            }

            CacheMaterial(renderer, *resolvedMaterial);
            CacheMesh(renderer, *resolvedMesh);
            const size_t passIndex = ResolveRenderPassIndex(*resolvedMaterial);
            auto& passBuckets = renderData.PassBuckets[passIndex];
            passBuckets[resolvedMaterial->Shaders.Id].push_back(toy);
            renderData.Drawables[toy->Handle.Id] = { resolvedMaterial, resolvedMesh };
        }

        return renderData;
    }

    void GraphicsPipeline::Draw(
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
            auto renderData = Prepare(renderer, FullStageView(stage->Root), aspectRatio);
            for (auto& pass : RenderPasses)
            {
                if (pass.Draw)
                {
                    pass.Draw(*this, renderer, renderData, pass);
                }
                else
                {
                    Draw(renderer, renderData, pass);
                }
            }
        }

        display.Context->Present();
        renderer.Backend->EndDraw();
    }

    void GraphicsPipeline::Draw(GraphicsRenderer& renderer, StageDrawData& renderData, const RenderPass& pass)
    {
        const auto passIndex = static_cast<size_t>(&pass - RenderPasses.data());
        if (passIndex >= RenderPasses.size())
        {
            TBX_ASSERT(false, "GraphicsPipeline: Render pass is not part of the pipeline.");
            return;
        }

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
                viewProjectionUniform.Data = camera.ViewProj;
                shaderResource->Upload(viewProjectionUniform);
                Draw(renderer, camera, bucket, shaderResource, renderData);
            }
        }
    }

    void GraphicsPipeline::Draw(
        GraphicsRenderer& renderer,
        const CameraData& camera,
        const std::vector<Ref<Toy>>& toys,
        const Ref<ShaderProgramResource>& shaderResource,
        const StageDrawData& renderData)
    {
        for (const auto& toyPtr : toys)
        {
            if (ShouldCull(toyPtr, camera.Frust, renderData))
            {
                continue;
            }

            const auto& toy = *toyPtr;

            Mat4x4 transformMatrix = Mat4x4::Identity;
            if (toy.Has<Transform>())
            {
                const auto transform = toy.Get<Transform>();
                transformMatrix = Mat4x4::FromTRS(transform->Position, transform->Rotation, transform->Scale);
            }

            ShaderUniform transformUniform = {};
            transformUniform.Name = TRANSFORM_UNIFORM_NAME;
            transformUniform.Data = transformMatrix;
            shaderResource->Upload(transformUniform);

            const auto drawDataIt = renderData.Drawables.find(toy.Handle.Id);
            if (drawDataIt == renderData.Drawables.end())
            {
                continue;
            }

            const auto& drawData = drawDataIt->second;
            if (!drawData.Mat || !drawData.Poly)
            {
                continue;
            }

            std::vector<UseGraphicsResourceScope> textureScopes = {};
            textureScopes.reserve(drawData.Mat->Textures.size());
            for (size_t textureIndex = 0; textureIndex < drawData.Mat->Textures.size(); ++textureIndex)
            {
                const auto& texture = drawData.Mat->Textures[textureIndex];
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

            const auto meshResourceIt = renderer.Cache.Meshes.find(drawData.Poly->Id);
            if (meshResourceIt == renderer.Cache.Meshes.end() || !meshResourceIt->second)
            {
                continue;
            }

            UseGraphicsResourceScope meshScope(meshResourceIt->second);
            meshResourceIt->second->Draw();
        }
    }

    bool GraphicsPipeline::ShouldCull(const Ref<Toy>& toy, const Frustum& frustum, const StageDrawData& renderData)
    {
        if (!toy)
        {
            return true;
        }

        if (toy->Has<Camera>())
        {
            return false;
        }

        const auto drawDataIt = renderData.Drawables.find(toy->Handle.Id);
        if (drawDataIt == renderData.Drawables.end() ||
            !drawDataIt->second.Mat ||
            !drawDataIt->second.Poly)
        {
            return true;
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
        CacheShaders(renderer, material.Shaders);

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
