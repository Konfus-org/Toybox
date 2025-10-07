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
    /// <summary>
    /// Represents a single renderable toy grouped by shader program usage.
    /// </summary>
    struct RenderBucketEntry
    {
        Ref<Toy> Toy = nullptr;
        const Material* MaterialBlock = nullptr;
        const Mesh* MeshBlock = nullptr;
    };

    using RenderBucket = std::vector<RenderBucketEntry>;
    using RenderBuckets = std::unordered_map<Uid, RenderBucket>;

    const std::string TRANSFORM_UNIFORM_NAME = "TransformUniform";

    /// <summary>
    /// Attempts to gather mesh and material information for a toy so it can be rendered.
    /// </summary>
    static bool TryGetRenderableEntry(const Ref<Toy>& toy, RenderBucketEntry& outEntry)
    {
        if (!toy)
        {
            return false;
        }

        if (toy->Blocks.Contains<Model>())
        {
            const auto& model = toy->Blocks.Get<Model>();
            outEntry = { toy, &model.Material, &model.Mesh };
            return true;
        }

        if (toy->Blocks.Contains<Mesh>() && toy->Blocks.Contains<Material>())
        {
            const auto& mesh = toy->Blocks.Get<Mesh>();
            const auto& material = toy->Blocks.Get<Material>();
            outEntry = { toy, &material, &mesh };
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
            RenderBucketEntry entry = {};
            if (!TryGetRenderableEntry(toy, entry))
            {
                continue;
            }

            if (!entry.MaterialBlock || !entry.MaterialBlock->ShaderProgram)
            {
                continue;
            }

            const auto shaderProgramId = entry.MaterialBlock->ShaderProgram->Id;
            if (shaderProgramId == Uid::Invalid)
            {
                continue;
            }

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

    GraphicsPipeline::GraphicsPipeline(
        const std::vector<Ref<IGraphicsBackend>>& backends,
        const std::vector<Ref<IGraphicsContextProvider>>& contextProviders,
        Ref<EventBus> eventBus)
        : _eventBus(eventBus)
        , _eventListener(eventBus)
    {
        TBX_ASSERT(!backends.empty(), "Rendering: requires at least one graphics backend instance.");
        TBX_ASSERT(_eventBus, "Rendering: requires a valid event bus instance.");

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

        _eventListener.Listen(this, &GraphicsPipeline::OnWindowOpened);
        _eventListener.Listen(this, &GraphicsPipeline::OnWindowClosed);
        _eventListener.Listen(this, &GraphicsPipeline::OnAppSettingsChanged);
        _eventListener.Listen(this, &GraphicsPipeline::OnStageOpened);
        _eventListener.Listen(this, &GraphicsPipeline::OnStageClosed);

        TBX_ASSERT(!_renderers.empty(), "Rendering: No compatible renderer implementations were provided for the available graphics APIs.");
    }

    GraphicsPipeline::~GraphicsPipeline() = default;

    void GraphicsPipeline::Update()
    {
        DrawFrame();
    }

    void GraphicsPipeline::DrawFrame()
    {
        GraphicsRenderer* renderer = TryGetRenderer(_activeGraphicsApi);
        if (!renderer || !renderer->Backend)
        {
            return;
        }

        RenderOpenStages(*renderer);
    }

    void GraphicsPipeline::RenderOpenStages(GraphicsRenderer& renderer)
    {
        if (!renderer.Backend)
        {
            return;
        }

        for (const auto& display : _openDisplays)
        {
            if (!display.Surface || !display.Context)
            {
                continue;
            }

            // Bind the display surface and prepare a clear framebuffer before processing stages.
            display.Context->MakeCurrent();
            renderer.Backend->BeginDraw({ {0, 0}, display.Surface->GetSize() });

            auto aspectRatio = CalculateAspectRatioFromSize(display.Surface->GetSize());

            for (const auto& stage : _openStages)
            {
                if (!stage)
                {
                    continue;
                }

                const auto stageRoot = stage->GetRoot();
                if (!stageRoot)
                {
                    continue;
                }

                FullStageView stageView(stageRoot);
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

                for (const auto& [shaderProgramId, bucket] : renderBuckets)
                {
                    if (bucket.empty())
                    {
                        continue;
                    }

                    const auto* representativeMaterial = bucket.front().MaterialBlock;
                    const bool shaderCached = renderer.Cache.Shaders.contains(shaderProgramId);
                    if (representativeMaterial && representativeMaterial->ShaderProgram && !shaderCached)
                    {
                        CompileShaders(representativeMaterial->ShaderProgram->Shaders);
                        CacheShaders(renderer, *representativeMaterial);
                    }

                    std::vector<const RenderBucketEntry*> visibleEntries = {};
                    std::vector<Ref<Texture>> texturesToCache = {};
                    std::vector<Ref<Mesh>> meshesToCache = {};
                    visibleEntries.reserve(bucket.size());
                    texturesToCache.reserve(bucket.size());
                    meshesToCache.reserve(bucket.size());

                    for (const auto& entry : bucket)
                    {
                        // Skip objects outside every camera frustum before we touch GPU caches.
                        if (ShouldCull(entry.Toy, frustums))
                        {
                            continue;
                        }

                        visibleEntries.push_back(&entry);

                        if (entry.MaterialBlock)
                        {
                            texturesToCache.insert(texturesToCache.end(), entry.MaterialBlock->Textures.begin(), entry.MaterialBlock->Textures.end());
                        }

                        if (entry.MeshBlock)
                        {
                            meshesToCache.push_back(MakeRef<Mesh>(*entry.MeshBlock));
                        }
                    }

                    if (visibleEntries.empty())
                    {
                        continue;
                    }

                    if (!texturesToCache.empty())
                    {
                        CacheTextures(renderer, texturesToCache);
                    }

                    if (!meshesToCache.empty())
                    {
                        CacheMeshes(renderer, meshesToCache);
                    }

                    const auto shaderResourceIt = renderer.Cache.Shaders.find(shaderProgramId);
                    if (shaderResourceIt == renderer.Cache.Shaders.end() || !shaderResourceIt->second)
                    {
                        continue;
                    }

                    UseGraphicsResourceScope shaderScope(shaderResourceIt->second);

                    for (const auto* entryPtr : visibleEntries)
                    {
                        if (!entryPtr)
                        {
                            continue;
                        }

                        const auto& entry = *entryPtr;

                        std::vector<Ref<TextureResource>> texturesToBind = {};
                        if (entry.MaterialBlock)
                        {
                            texturesToBind.reserve(entry.MaterialBlock->Textures.size());
                            for (size_t textureIndex = 0; textureIndex < entry.MaterialBlock->Textures.size(); ++textureIndex)
                            {
                                const auto& texture = entry.MaterialBlock->Textures[textureIndex];
                                if (!texture)
                                {
                                    continue;
                                }

                                const auto textureResourceIt = renderer.Cache.Textures.find(texture->Id);
                                if (textureResourceIt == renderer.Cache.Textures.end() || !textureResourceIt->second)
                                {
                                    continue;
                                }

                                textureResourceIt->second->SetSlot(static_cast<uint32>(textureIndex));
                                texturesToBind.push_back(textureResourceIt->second);
                            }
                        }

                        std::vector<UseGraphicsResourceScope> textureScopes = {};
                        textureScopes.reserve(texturesToBind.size());
                        for (const auto& textureResource : texturesToBind)
                        {
                            textureScopes.emplace_back(textureResource);
                        }

                        if (!entry.MeshBlock)
                        {
                            continue;
                        }

                        const auto meshResourceIt = renderer.Cache.Meshes.find(entry.MeshBlock->Id);
                        if (meshResourceIt == renderer.Cache.Meshes.end() || !meshResourceIt->second)
                        {
                            continue;
                        }

                        meshResourceIt->second->SetVertexBuffer(entry.MeshBlock->Vertices);
                        meshResourceIt->second->SetIndexBuffer(entry.MeshBlock->Indices);
                        UseGraphicsResourceScope meshScope(meshResourceIt->second);

                        Mat4x4 transformMatrix = Mat4x4::Identity;
                        if (entry.Toy && entry.Toy->Blocks.Contains<Transform>())
                        {
                            const auto& transform = entry.Toy->Blocks.Get<Transform>();
                            transformMatrix = Mat4x4::FromTRS(transform.Position, transform.Rotation, transform.Scale);
                        }

                        ShaderUniform transformUniform = {};
                        transformUniform.Name = TRANSFORM_UNIFORM_NAME;
                        transformUniform.Data = transformMatrix;
                        shaderResourceIt->second->Upload(transformUniform);
                    }
                }
            }

            renderer.Backend->EndDraw(_clearColor);
            display.Context->Present();
        }

        if (_eventBus)
        {
            _eventBus->Send(RenderedFrameEvent());
        }
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
            if (renderer.Backend)
            {
                renderer.Backend->SetContext(nullptr);
            }

            renderer.Cache.Clear();
        }

        for (auto& display : _openDisplays)
        {
            display.Context = nullptr;
        }

        GraphicsRenderer* renderer = TryGetRenderer(_activeGraphicsApi);
        if (!renderer || !renderer->Backend || !renderer->ContextProvider)
        {
            return;
        }

        renderer->Cache.Clear();

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

    void GraphicsPipeline::CompileShaders(const std::vector<Ref<Shader>>& shaders)
    {
        if (shaders.empty())
        {
            return;
        }

        auto* renderer = TryGetRenderer(_activeGraphicsApi);
        if (!renderer || !renderer->Backend)
        {
            return;
        }

        std::vector<Ref<Shader>> validShaders = {};
        validShaders.reserve(shaders.size());
        for (const auto& shader : shaders)
        {
            if (shader)
            {
                validShaders.push_back(shader);
            }
        }

        if (validShaders.empty())
        {
            return;
        }

        renderer->Backend->CompileShaders(validShaders);
    }

    void GraphicsPipeline::CacheShaders(GraphicsRenderer& renderer, const Material& material)
    {
        if (!renderer.Backend || !material.ShaderProgram)
        {
            return;
        }

        const auto shaderProgramId = material.ShaderProgram->Id;
        if (shaderProgramId == Uid::Invalid)
        {
            return;
        }

        if (renderer.Cache.Shaders.contains(shaderProgramId))
        {
            return;
        }

        std::vector<Ref<Shader>> program = {};
        program.reserve(material.ShaderProgram->Shaders.size());
        for (const auto& shader : material.ShaderProgram->Shaders)
        {
            if (shader)
            {
                program.push_back(shader);
            }
        }

        if (program.empty())
        {
            return;
        }

        auto resource = renderer.Backend->CreateResource(program);
        TBX_ASSERT(resource, "Rendering: Unable to cache shader program for active graphics api.");
        if (!resource)
        {
            return;
        }

        resource->RenderId = shaderProgramId;
        renderer.Cache.Shaders.emplace(shaderProgramId, resource);
    }

    void GraphicsPipeline::CacheTextures(GraphicsRenderer& renderer, const std::vector<Ref<Texture>>& textures)
    {
        if (!renderer.Backend || textures.empty())
        {
            return;
        }

        for (const auto& texture : textures)
        {
            if (!texture || texture->Id == Uid::Invalid)
            {
                continue;
            }

            if (renderer.Cache.Textures.contains(texture->Id))
            {
                continue;
            }

            auto resource = renderer.Backend->CreateResource(texture);
            TBX_ASSERT(resource, "Rendering: Unable to cache textures for active graphics api.");
            if (!resource)
            {
                continue;
            }

            resource->RenderId = texture->Id;
            renderer.Cache.Textures.emplace(texture->Id, resource);
        }
    }

    void GraphicsPipeline::CacheMeshes(GraphicsRenderer& renderer, const std::vector<Ref<Mesh>>& meshes)
    {
        if (!renderer.Backend || meshes.empty())
        {
            return;
        }

        for (const auto& mesh : meshes)
        {
            if (!mesh || mesh->Id == Uid::Invalid)
            {
                continue;
            }

            if (renderer.Cache.Meshes.contains(mesh->Id))
            {
                continue;
            }

            auto resource = renderer.Backend->CreateResource(mesh);
            TBX_ASSERT(resource, "Rendering: Unable to cache meshes for active graphics api.");
            if (!resource)
            {
                continue;
            }

            resource->RenderId = mesh->Id;
            renderer.Cache.Meshes.emplace(mesh->Id, resource);
        }
    }

    void GraphicsPipeline::OnAppSettingsChanged(const AppSettingsChangedEvent& e)
    {
        const auto& newSettings = e.GetNewSettings();
        _clearColor = newSettings.ClearColor;
        auto desiredApi = newSettings.RenderingApi;

        const GraphicsRenderer* desiredRenderer = TryGetRenderer(desiredApi);
        const bool rendererAvailable = desiredRenderer && desiredRenderer->Backend && desiredRenderer->ContextProvider;
        if (!rendererAvailable)
        {
            GraphicsApi fallbackApi = GraphicsApi::None;
            for (const auto& [api, renderer] : _renderers)
            {
                if (!renderer.Backend || !renderer.ContextProvider)
                {
                    continue;
                }

                fallbackApi = api;
                break;
            }

            desiredApi = fallbackApi;
        }

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

        GraphicsRenderer* renderer = TryGetRenderer(_activeGraphicsApi);
        if (!renderer || !renderer->Backend)
        {
            return;
        }

        if (!renderer->ContextProvider)
        {
            return;
        }

        auto displayIt = std::find_if(_openDisplays.begin(), _openDisplays.end(), [newWindow](const GraphicsDisplay& display)
        {
            return display.Surface == newWindow;
        });

        auto context = renderer->ContextProvider->Provide(newWindow);
        TBX_ASSERT(context, "Rendering: Failed to create a graphics context for the opened window.");
        if (!context)
        {
            return;
        }

        renderer->Backend->SetContext(context);

        if (displayIt != _openDisplays.end())
        {
            displayIt->Context = context;
        }
        else
        {
            _openDisplays.push_back({ newWindow, context });
        }
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

        if (_openDisplays.empty())
        {
            GraphicsRenderer* renderer = TryGetRenderer(_activeGraphicsApi);
            if (renderer && renderer->Backend)
            {
                renderer->Backend->SetContext(nullptr);
                renderer->Cache.Clear();
            }
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

    GraphicsRenderer* GraphicsPipeline::TryGetRenderer(GraphicsApi api)
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
}

