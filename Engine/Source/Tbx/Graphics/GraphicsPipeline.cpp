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
#include "Tbx/Events/RenderEvents.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Stages/Stage.h"
#include <algorithm>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace Tbx
{
    ///////////// HELPERS/UTILS //////////////////

    const std::string TRANSFORM_UNIFORM_NAME = "TransformUniform";
    const std::string VIEW_PROJECTION_UNIFORM_NAME = "ViewProjectionUniform";

    using RenderBucket = std::vector<Ref<Toy>>;
    using RenderBuckets = std::unordered_map<Uid, RenderBucket>;

    /// <summary>
    /// Attempts to gather mesh and material information for a toy so it can be rendered.
    /// </summary>
    static bool TryGetRenderableEntry(const Ref<Toy>& toy, Ref<Toy>& outEntry)
    {
        if (!toy)
        {
            return false;
        }

        if (toy->Blocks.Contains<Model>())
        {
            const auto& model = toy->Blocks.Get<Model>();
            outEntry = toy;
            return true;
        }

        if (toy->Blocks.Contains<Mesh>() && toy->Blocks.Contains<Material>())
        {
            outEntry = toy;
            return true;
        }

        return false;
    }

    /// <summary>
    /// Collects renderable toys into buckets keyed by shader program, ensuring resources are prepared per program.
    /// </summary>
    static RenderBuckets BuildRenderBuckets(const FullStageView& stageView)
    {
        RenderBuckets buckets = {};

        for (const auto& toy : stageView)
        {
            Ref<Toy> entry = nullptr;
            if (!TryGetRenderableEntry(toy, entry))
            {
                continue;
            }

            if (!entry->Blocks.Contains<Material>())
            {
                continue;
            }

            const auto& materialBlock = entry->Blocks.Get<Material>();
            if (materialBlock.ShaderProgram.Id != Uid::Invalid)
            {
                continue;
            }

            const auto shaderProgramId = materialBlock.ShaderProgram.Id;
            buckets[shaderProgramId].push_back(entry);
        }

        return buckets;
    }

    /// <summary>
    /// Builds view frustums for every active camera, updating their aspect ratio to match the current display.
    /// </summary>
    static std::vector<Frustum> BuildCameraFrustums(const FullStageView& stageView, float aspectRatio)
    {
        std::vector<Frustum> frustums = {};

        for (const auto& toy : stageView)
        {
            if (!toy || !toy->Blocks.Contains<Camera>())
            {
                continue;
            }

            auto& camera = toy->Blocks.Get<Camera>();
            TBX_ASSERT(aspectRatio > 0.0f, "GraphicsPipeline: aspect ratio must be positive.");
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

            frustums.push_back(Camera::CalculateFrustum(camPos, camRot, camera.GetProjectionMatrix()));
        }

        return frustums;
    }

    ///////////// GRAPHICS PIPELINE //////////////////

    GraphicsPipeline::GraphicsPipeline(
        const std::vector<Ref<IGraphicsBackend>>& backends,
        const std::vector<Ref<IGraphicsContextProvider>>& contextProviders,
        Ref<EventBus> eventBus)
        : _eventBus(eventBus)
        , _eventListener(eventBus)
    {
        TBX_ASSERT(!backends.empty(), "Rendering: requires at least one graphics backend instance.");
        TBX_ASSERT(_eventBus, "Rendering: requires a valid event bus instance.");

        _eventListener.Listen(this, &GraphicsPipeline::OnWindowOpened);
        _eventListener.Listen(this, &GraphicsPipeline::OnWindowClosed);
        _eventListener.Listen(this, &GraphicsPipeline::OnAppSettingsChanged);
        _eventListener.Listen(this, &GraphicsPipeline::OnStageOpened);
        _eventListener.Listen(this, &GraphicsPipeline::OnStageClosed);

        InitializeRenderers(backends, contextProviders);
        TBX_ASSERT(!_renderers.empty(), "Rendering: No compatible renderer implementations were provided for the available graphics APIs.");
    }

    void GraphicsPipeline::InitializeRenderers(const std::vector<Tbx::Ref<Tbx::IGraphicsBackend>>& backends, const std::vector<Tbx::Ref<Tbx::IGraphicsContextProvider>>& contextProviders)
    {
        for (const auto& backend : backends)
        {
            if (!backend)
            {
                continue;
            }

            const auto api = backend->GetApi();
            if (api == GraphicsApi::None)
            {
                continue;
            }

            auto& renderer = _renderers[api];
            if (!renderer.Backend)
            {
                renderer.Backend = backend;
            }
        }

        for (const auto& provider : contextProviders)
        {
            if (!provider)
            {
                continue;
            }

            const auto api = provider->GetApi();
            if (api == GraphicsApi::None)
            {
                continue;
            }

            auto rendererIt = _renderers.find(api);
            if (rendererIt == _renderers.end())
            {
                continue;
            }

            auto& renderer = rendererIt->second;
            if (!renderer.ContextProvider)
            {
                renderer.ContextProvider = provider;
            }
        }

        for (auto it = _renderers.begin(); it != _renderers.end();)
        {
            auto& renderer = it->second;
            if (!renderer.Backend || !renderer.ContextProvider)
            {
                it = _renderers.erase(it);
                continue;
            }
            ++it;
        }
    }

    GraphicsRenderer* GraphicsPipeline::GetRenderer(GraphicsApi api)
    {
        if (api == GraphicsApi::None)
        {
            return nullptr;
        }

        auto rendererIt = _renderers.find(api);
        if (rendererIt == _renderers.end())
        {
            return nullptr;
        }

        return &rendererIt->second;
    }

    void GraphicsPipeline::Update()
    {
        DrawFrame();
    }

    void GraphicsPipeline::DrawFrame()
    {
        auto* renderer = GetRenderer(_activeGraphicsApi);
        if (!renderer)
        {
            return;
        }

        RenderOpenStages(*renderer);
    }

    void GraphicsPipeline::RenderOpenStages(GraphicsRenderer& renderer)
    {
        for (const auto& display : _openDisplays)
        {
            if (!display.Surface || !display.Context)
            {
                continue;
            }

            // Start drawing
            auto aspectRatio = display.Surface->GetSize().GetAspectRatio();
            auto viewport = Viewport({0, 0}, display.Surface->GetSize());
            display.Context->MakeCurrent();
            renderer.Backend->SetContext(display.Context);
            renderer.Backend->BeginDraw(_clearColor, viewport);
            //_eventBus->Send(RenderBeginDrawEvent());

            // Draw
            for (const auto& stage : _openStages)
            {
                auto stageView = FullStageView(stage->GetRoot());

                // Cameras define the visible region for this stage.
                auto frustums = BuildCameraFrustums(stageView, aspectRatio);
                if (frustums.empty())
                {
                    continue;
                }

                // Group renderable toys so that shader, mesh, and texture caching happens per program.
                auto renderBuckets = BuildRenderBuckets(stageView);
                if (renderBuckets.empty())
                {
                    continue;
                }

                // Render things per bucket
                for (const auto& [shaderProgramId, bucket] : renderBuckets)
                {
                    if (bucket.empty())
                    {
                        continue;
                    }

                    // Cull what isn't visiable and cache what isn't cached yet.
                    std::vector<Ref<Toy>> visibleEntities = {};
                    {
                        for (const auto& entity : bucket)
                        {
                            // Skip objects outside every camera frustum before we touch GPU caches.
                            if (ShouldCull(entity, frustums))
                            {
                                continue;
                            }

                            // Cache resources for this entity
                            if (entity->Blocks.Contains<Mesh>() &&
                                entity->Blocks.Contains<Material>())
                            {
                                CacheMaterial(renderer, entity->Blocks.Get<Material>());
                                CacheMesh(renderer, entity->Blocks.Get<Mesh>());
                            }

                            // Passed culling was cached, add to visible entities
                            visibleEntities.push_back(entity);
                        }
                        if (visibleEntities.empty())
                        {
                            continue;
                        }
                    }

                    // Bind to the active shader
                    const auto shaderResource = renderer.Cache.ShaderPrograms[shaderProgramId];
                    UseGraphicsResourceScope shaderScope(shaderResource);

                    // Draw entities
                    for (const auto& entityPtr : visibleEntities)
                    {
                        const auto& entity = *entityPtr;

                        // Upload camera uniforms
                        if (entity.Blocks.Contains<Camera>())
                        {
                            const auto& camera = entity.Blocks.Get<Camera>();

                            Vector3 camPos = Vector3::Zero;
                            Quaternion camRot = Quaternion::Identity;
                            if (entity.Blocks.Contains<Transform>())
                            {
                                const auto& camTransform = entity.Blocks.Get<Transform>();
                                camPos = camTransform.Position;
                                camRot = camTransform.Rotation;
                            }

                            auto viewProjMatrix = camera.CalculateViewProjectionMatrix(camPos, camRot, camera.GetProjectionMatrix());
                            ShaderUniform transformUniform = {};
                            transformUniform.Name = VIEW_PROJECTION_UNIFORM_NAME;
                            transformUniform.Data = viewProjMatrix;
                            shaderResource->Upload(transformUniform);
                        }

                        // TODO: make material instances that share the same shader program and textures then we can bind to it once per bucket
                        // Use the the entities material for drawing
                        std::vector<Ref<TextureResource>> texturesToBind = {};
                        {
                            const auto& material = entity.Blocks.Get<Material>();
                            texturesToBind.reserve(material.Textures.size());
                            for (size_t textureIndex = 0; textureIndex < material.Textures.size(); ++textureIndex)
                            {
                                const auto& texture = material.Textures[textureIndex];
                                if (!texture)
                                {
                                    continue;
                                }

                                const auto textureResource = renderer.Cache.Textures[texture->Id];
                                textureResource->SetSlot(static_cast<uint32>(textureIndex));
                                texturesToBind.push_back(textureResource);
                            }

                            std::vector<UseGraphicsResourceScope> textureScopes = {};
                            textureScopes.reserve(texturesToBind.size());
                            for (const auto& textureResource : texturesToBind)
                            {
                                textureScopes.emplace_back(textureResource);
                            }
                        }

                        // Bind to the entities mesh and upload its transform info
                        const auto& mesh = entity.Blocks.Get<Mesh>();
                        const auto meshResource = renderer.Cache.Meshes[mesh.Id];
                        UseGraphicsResourceScope meshScope(meshResource);
                        {
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
                        }
                    }
                }
            }

            // End drawing
            renderer.Backend->EndDraw();
            //_eventBus->Send(RenderEndDrawEvent());
            display.Context->Present();
        }

        _eventBus->Send(RenderedFrameEvent());
    }

    bool GraphicsPipeline::ShouldCull(const Ref<Toy>& toy, const std::vector<Frustum>& frustums)
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

        if (frustums.empty())
        {
            return true;
        }

        const auto sphere = BoundingSphere(toy);
        for (const auto& frustum : frustums)
        {
            if (frustum.Intersects(sphere))
            {
                return false;
            }
        }

        return true;
    }

    void GraphicsPipeline::AddStage(const Ref<Stage>& stage)
    {
        if (!stage)
        {
            return;
        }

        const auto it = std::find(_openStages.begin(), _openStages.end(), stage);
        if (it != _openStages.end())
        {
            return;
        }

        _openStages.push_back(stage);
    }

    void GraphicsPipeline::RemoveStage(const Ref<Stage>& stage)
    {
        if (!stage)
        {
            return;
        }

        auto it = std::find(_openStages.begin(), _openStages.end(), stage);
        if (it != _openStages.end())
        {
            _openStages.erase(it);
        }
    }

    void GraphicsPipeline::RecreateRenderersForCurrentApi()
    {
        for (auto& [api, renderer] : _renderers)
        {
            renderer.Cache.Clear();
        }

        for (auto& display : _openDisplays)
        {
            display.Context = nullptr;
        }

        auto* renderer = GetRenderer(_activeGraphicsApi);
        if (!renderer)
        {
            return;
        }

        for (auto& display : _openDisplays)
        {
            if (!display.Surface)
            {
                continue;
            }

            auto context = renderer->ContextProvider->Provide(display.Surface);
            TBX_ASSERT(context, "Rendering: Failed to recreate a graphics context for an open window.");
            if (!context)
            {
                continue;
            }

            renderer->Backend->SetContext(context);
            display.Context = context;
        }
    }

    void GraphicsPipeline::CacheShaders(GraphicsRenderer& renderer, const ShaderProgram& shaderProgram)
    {
        for (const auto& shader : shaderProgram.Shaders)
        {
            if (renderer.Cache.Shaders.contains(shader->Id))
            {
                continue;
            }

            auto compiledShader = renderer.Backend->CompileShader(*shader);
            TBX_ASSERT(compiledShader, "Rendering: Unable to cache shaders for active graphics api.");
            if (!compiledShader)
            {
                continue;
            }
            renderer.Cache.Shaders[shader->Id] = compiledShader;
        }
    }

    void GraphicsPipeline::CacheMaterial(GraphicsRenderer& renderer, const Material& material)
    {
        // Cache material textures
        for (const auto& texture : material.Textures)
        {
            if (renderer.Cache.Textures.contains(texture->Id))
            {
                continue;
            }

            auto resource = renderer.Backend->UploadTexture(*texture);
            TBX_ASSERT(resource, "Rendering: Unable to cache textures for active graphics api.");
            if (!resource)
            {
                continue;
            }

            resource->RenderId = texture->Id;
            renderer.Cache.Textures.emplace(texture->Id, resource);
        }

        // Cache material shader program
        {
            const auto shaderProgramId = material.ShaderProgram.Id;
            if (renderer.Cache.ShaderPrograms.contains(shaderProgramId))
            {
                return;
            }

            const auto& shaders = material.ShaderProgram.Shaders;
            CacheShaders(renderer, shaders);

            std::vector<Ref<ShaderResource>> shaderResources = {};
            for (const auto& shader : shaders)
            {
                auto shaderResource = renderer.Cache.Shaders[shader->Id];
                shaderResources.push_back(shaderResource);
            }

            auto resource = renderer.Backend->CreateShaderProgram(shaderResources);
            TBX_ASSERT(resource, "Rendering: Unable to cache shader program for active graphics api.");
            if (!resource)
            {
                return;
            }

            resource->RenderId = shaderProgramId;
            renderer.Cache.ShaderPrograms.emplace(shaderProgramId, resource);
        }
    }

    void GraphicsPipeline::CacheMesh(GraphicsRenderer& renderer, const Mesh& mesh)
    {
        if (renderer.Cache.Meshes.contains(mesh.Id))
        {
            return;
        }

        auto resource = renderer.Backend->UploadMesh(mesh);
        TBX_ASSERT(resource, "Rendering: Unable to cache meshes for active graphics api.");
        if (!resource)
        {
            return;
        }

        resource->RenderId = mesh.Id;
        renderer.Cache.Meshes.emplace(mesh.Id, resource);
    }

    void GraphicsPipeline::OnAppSettingsChanged(const AppSettingsChangedEvent& e)
    {
        const auto& newSettings = e.GetNewSettings();
        auto desiredApi = newSettings.RenderingApi;

        _clearColor = newSettings.ClearColor;
        if (_activeGraphicsApi != desiredApi)
        {
            _activeGraphicsApi = desiredApi;
            RecreateRenderersForCurrentApi();
        }
    }

    void GraphicsPipeline::OnWindowOpened(const WindowOpenedEvent& e)
    {
        auto newWindow = e.GetWindow();
        if (!newWindow)
        {
            TBX_ASSERT(newWindow, "Rendering: Invalid window open event, ensure a valid window is passed!");
            return;
        }

        auto* renderer = GetRenderer(_activeGraphicsApi);
        if (!renderer)
        {
            return;
        }

        auto context = renderer->ContextProvider->Provide(newWindow);
        TBX_ASSERT(context, "Rendering: Failed to create a graphics context for the opened window.");
        if (!context)
        {
            return;
        }

        _openDisplays.emplace_back(newWindow, context);
    }

    void GraphicsPipeline::OnWindowClosed(const WindowClosedEvent& e)
    {
        auto closedWindow = e.GetWindow();
        if (!closedWindow)
        {
            return;
        }

        auto displayIt = std::find_if(_openDisplays.begin(), _openDisplays.end(), [closedWindow](const GraphicsDisplay& display)
        {
            return display.Surface == closedWindow;
        });
        if (displayIt != _openDisplays.end())
        {
            _openDisplays.erase(displayIt);
        }
        else
        {
            TBX_ASSERT(false, "Rendering: A renderer could not be found for the window that was closed!");
        }
    }

    void GraphicsPipeline::OnStageOpened(const StageOpenedEvent& e)
    {
        AddStage(e.GetStage());
    }

    void GraphicsPipeline::OnStageClosed(const StageClosedEvent& e)
    {
        RemoveStage(e.GetStage());
    }
}

