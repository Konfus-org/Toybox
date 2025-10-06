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
        const std::vector<Ref<IManageGraphicsApis>>& apiManagers,
        const std::vector<Ref<IGraphicsResourceFactory>>& resourceFactories,
        const std::vector<Ref<IGraphicsContextProvider>>& contextProviders,
        const std::vector<Ref<IShaderCompiler>>& shaderCompilers,
        Ref<EventBus> eventBus)
        : _eventBus(eventBus)
        , _eventListener(eventBus)
    {
        TBX_ASSERT(!resourceFactories.empty(), "Rendering: requires at least one renderer factory instance.");
        TBX_ASSERT(!contextProviders.empty(), "Rendering: requires at least one graphics context provider instance.");
        TBX_ASSERT(!shaderCompilers.empty(), "Rendering: requires at least one shader compiler instance.");
        TBX_ASSERT(_eventBus, "Rendering: requires a valid event bus instance.");

        auto findSupporting = [](const auto& providers, GraphicsApi api)
        {
            using ProviderType = typename std::decay_t<decltype(providers)>::value_type::element_type;
            Ref<ProviderType> result = nullptr;
            auto it = std::find_if(providers.begin(), providers.end(), [api](const auto& candidate)
            {
                if (!candidate)
                {
                    return false;
                }

                const auto supportedApis = candidate->GetSupportedApis();
                return std::find(supportedApis.begin(), supportedApis.end(), api) != supportedApis.end();
            });

            if (it != providers.end())
            {
                result = *it;
            }

            return result;
        };

        for (const auto& manager : apiManagers)
        {
            if (!manager)
            {
                continue;
            }

            const auto supportedApis = manager->GetSupportedApis();
            for (auto supportedApi : supportedApis)
            {
                if (_renderers.contains(supportedApi))
                {
                    continue;
                }

                auto resourceFactory = findSupporting(resourceFactories, supportedApi);
                auto contextProvider = findSupporting(contextProviders, supportedApi);
                auto shaderCompiler = findSupporting(shaderCompilers, supportedApi);

                if (!resourceFactory || !contextProvider || !shaderCompiler)
                {
                    continue;
                }

                auto renderer = MakeRef<GraphicsRenderer>();
                renderer->ApiManager = manager;
                renderer->ResourceFactory = resourceFactory;
                renderer->ContextProvider = contextProvider;
                renderer->ShaderCompiler = shaderCompiler;

                _renderers.emplace(supportedApi, renderer);
            }
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
        Ref<GraphicsRenderer> renderer = nullptr;
        if (!TryGetRenderer(_currApi, renderer) || !renderer)
        {
            return;
        }

        RenderOpenStages(renderer);
    }

    void GraphicsPipeline::RenderOpenStages(const Ref<GraphicsRenderer>& renderer)
    {
        for (const auto& display : _openDisplays)
        {
            if (!display.Surface || !display.Context)
            {
                continue;
            }

            // Bind the display surface and prepare a clear framebuffer before processing stages.
            display.Context->MakeCurrent();
            display.Context->SetViewport({ {0, 0}, display.Surface->GetSize() });
            display.Context->SetResolution(display.Surface->GetSize());
            display.Context->SetClearColor(_clearColor);
            display.Context->Clear();

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
                    const bool shaderCached = renderer && renderer->ResourceCache.Shaders.contains(shaderProgramId);
                    if (representativeMaterial && representativeMaterial->ShaderProgram && !shaderCached)
                    {
                        CompileShaders(renderer, representativeMaterial->ShaderProgram->Shaders);
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

                    if (!renderer)
                    {
                        continue;
                    }

                    const auto shaderResourceIt = renderer->ResourceCache.Shaders.find(shaderProgramId);
                    if (shaderResourceIt == renderer->ResourceCache.Shaders.end() || !shaderResourceIt->second)
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

                                const auto textureResourceIt = renderer->ResourceCache.Textures.find(texture->Id);
                                if (textureResourceIt == renderer->ResourceCache.Textures.end() || !textureResourceIt->second)
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

                        const auto meshResourceIt = renderer->ResourceCache.Meshes.find(entry.MeshBlock->Id);
                        if (meshResourceIt == renderer->ResourceCache.Meshes.end() || !meshResourceIt->second)
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

            display.Context->SwapBuffers();
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
            if (renderer && renderer->ApiManager)
            {
                renderer->ApiManager->Shutdown();
            }

            if (renderer)
            {
                renderer->ResourceCache.Clear();
            }
        }

        for (auto& display : _openDisplays)
        {
            display.Context = nullptr;
        }

        Ref<GraphicsRenderer> renderer = nullptr;
        if (!TryGetRenderer(_currApi, renderer) || !renderer || !renderer->ContextProvider)
        {
            return;
        }

        for (auto& display : _openDisplays)
        {
            if (!display.Surface)
            {
                continue;
            }

            display.Context = renderer->ContextProvider->Provide(display.Surface, _currApi);
            TBX_ASSERT(display.Context, "Rendering: Failed to recreate a graphics context for an open window.");
            if (!display.Context)
            {
                continue;
            }

            if (renderer->ApiManager)
            {
                renderer->ApiManager->Initialize(display.Context, _currApi);
            }
        }
    }

    void GraphicsPipeline::CompileShaders(const Ref<GraphicsRenderer>& renderer, const std::vector<Ref<Shader>>& shaders)
    {
        if (!renderer || !renderer->ShaderCompiler || shaders.empty())
        {
            return;
        }

        for (const auto& shader : shaders)
        {
            if (!shader)
            {
                continue;
            }

            renderer->ShaderCompiler->Compile(shader, _currApi);
        }
    }

    void GraphicsPipeline::CacheShaders(const Ref<GraphicsRenderer>& renderer, const Material& material)
    {
        if (!renderer || !renderer->ResourceFactory || !material.ShaderProgram)
        {
            return;
        }

        const auto shaderProgramId = material.ShaderProgram->Id;
        if (shaderProgramId == Uid::Invalid)
        {
            return;
        }

        if (renderer->ResourceCache.Shaders.contains(shaderProgramId))
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

        auto resource = renderer->ResourceFactory->Create(program);
        TBX_ASSERT(resource, "Rendering: Unable to cache shader program for active graphics api.");
        if (!resource)
        {
            return;
        }

        resource->RenderId = shaderProgramId;
        renderer->ResourceCache.Shaders.emplace(shaderProgramId, resource);
    }

    void GraphicsPipeline::CacheTextures(const Ref<GraphicsRenderer>& renderer, const std::vector<Ref<Texture>>& textures)
    {
        if (!renderer || !renderer->ResourceFactory || textures.empty())
        {
            return;
        }

        for (const auto& texture : textures)
        {
            if (!texture || texture->Id == Uid::Invalid)
            {
                continue;
            }

            if (renderer->ResourceCache.Textures.contains(texture->Id))
            {
                continue;
            }

            auto resource = renderer->ResourceFactory->Create(texture);
            TBX_ASSERT(resource, "Rendering: Unable to cache textures for active graphics api.");
            if (!resource)
            {
                continue;
            }

            resource->RenderId = texture->Id;
            renderer->ResourceCache.Textures.emplace(texture->Id, resource);
        }
    }

    void GraphicsPipeline::CacheMeshes(const Ref<GraphicsRenderer>& renderer, const std::vector<Ref<Mesh>>& meshes)
    {
        if (!renderer || !renderer->ResourceFactory || meshes.empty())
        {
            return;
        }

        for (const auto& mesh : meshes)
        {
            if (!mesh || mesh->Id == Uid::Invalid)
            {
                continue;
            }

            if (renderer->ResourceCache.Meshes.contains(mesh->Id))
            {
                continue;
            }

            auto resource = renderer->ResourceFactory->Create(mesh);
            TBX_ASSERT(resource, "Rendering: Unable to cache meshes for active graphics api.");
            if (!resource)
            {
                continue;
            }

            resource->RenderId = mesh->Id;
            renderer->ResourceCache.Meshes.emplace(mesh->Id, resource);
        }
    }

    void GraphicsPipeline::OnAppSettingsChanged(const AppSettingsChangedEvent& e)
    {
        const auto& newSettings = e.GetNewSettings();
        _resolution = newSettings.Resolution;
        _clearColor = newSettings.ClearColor;
        if (_currApi != newSettings.RenderingApi)
        {
            _currApi = newSettings.RenderingApi;
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

        Ref<GraphicsRenderer> renderer = nullptr;
        if (!TryGetRenderer(_currApi, renderer) || !renderer || !renderer->ContextProvider)
        {
            return;
        }

        auto displayIt = std::find_if(_openDisplays.begin(), _openDisplays.end(), [newWindow](const GraphicsDisplay& display)
        {
            return display.Surface == newWindow;
        });

        auto context = renderer->ContextProvider->Provide(newWindow, _currApi);
        TBX_ASSERT(context, "Rendering: Failed to create a graphics context for the opened window.");
        if (!context)
        {
            return;
        }

        if (renderer->ApiManager)
        {
            renderer->ApiManager->Initialize(context, _currApi);
        }

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
            Ref<GraphicsRenderer> renderer = nullptr;
            if (TryGetRenderer(_currApi, renderer) && renderer && renderer->ApiManager)
            {
                renderer->ApiManager->Shutdown();
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

    bool GraphicsPipeline::TryGetRenderer(GraphicsApi api, Ref<GraphicsRenderer>& renderer)
    {
        if (api == GraphicsApi::None)
        {
            renderer = nullptr;
            return false;
        }

        auto rendererIt = _renderers.find(api);
        if (rendererIt == _renderers.end())
        {
            renderer = nullptr;
            return false;
        }

        renderer = rendererIt->second;
        return renderer != nullptr;
    }
}

