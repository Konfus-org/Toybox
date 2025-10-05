#include "Tbx/PCH.h"
#include "Tbx/Graphics/GraphicsPipeline.h"
#include "Tbx/Debug/Asserts.h"
#include "Tbx/Graphics/Model.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Texture.h"
#include "Tbx/Graphics/Camera.h"
#include "Tbx/Stages/Views.h"
#include "Tbx/Math/Transform.h"
#include "Tbx/Graphics/Frustum.h"
#include "Tbx/Graphics/Sphere.h"
#include "Tbx/Events/RenderEvents.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Stages/Stage.h"
#include <algorithm>
#include <type_traits>
#include <utility>

namespace Tbx
{
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

            display.Context->MakeCurrent();
            display.Context->SetViewport({ {0, 0}, display.Surface->GetSize() });
            display.Context->SetResolution(display.Surface->GetSize());
            display.Context->SetClearColor(_clearColor);
            display.Context->Clear();

            auto aspectRatio = CalculateAspectRatioFromSize(display.Surface->GetSize());

            // TODO: Break this out into something that is more performant and testable...
            // Like a RenderGraph/SceneGraph thing....
            for (const auto& stage : _openStages)
            {
                if (!stage)
                {
                    continue;
                }

                const auto spaceRoot = stage->GetRoot();
                if (!spaceRoot)
                {
                    continue;
                }

                // Build view frustums from cameras
                auto frustums = std::vector<Frustum>();
                auto stageView = FullStageView(stage);

                // TODO: Sort into render buckets based on shader program... 
                // if a thing doesn't have a mesh and material or a model (mesh and material bundled) then it doesn't get added to the renderBuckets.
                for (const auto& toy : stageView)
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
                if (frustums.empty()) continue;

                // TODO: Replace this with iterating over render buckets and culling what isn't in view
                // For each shader program in our buck set it active via a scope while we iterate the buckets contents
                // For each mesh set it active via a scope inside our bucket iteration
                // For each material grab its textures, set their slot based on index and then active them.
                // For each model do the above for its mesh and material
                for (const auto& toy : stageView)
                {
                    // Cull things that aren't in view
                    int retFlag;
                    ShouldCull(toy, frustums, retFlag);
                    if (retFlag == 3) continue;

                    // Materials themselves aren't allowed
                    {
                        const bool hasShaderProgram = toy->Blocks.Contains<ShaderProgram>();
                        TBX_ASSERT(!hasShaderProgram, "Toys should only use materials, not shader programs!");
                        if (hasShaderProgram) continue;
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

    bool GraphicsPipeline::ShouldCull(const Tbx::Ref<Tbx::Toy>& toy, std::vector<Tbx::Frustum>& frustums)
    {
        if (!toy->Blocks.Contains<Camera>())
        {
            const auto sphere = BoundingSphere(toy);

            // Check if the sphere is in at least one of our frustums
            bool inFrustum = false;
            for (const auto& frustum : frustums)
            {
                if (!frustum.Intersects(sphere)) continue;

                return false;
                break;
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
                renderer->ResourceCache.clear();
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
        if (!renderer || !renderer->ResourceFactory || material.Id == Uid::Invalid)
        {
            return;
        }

        if (renderer->ResourceCache.contains(material.Id))
        {
            return;
        }

        std::vector<Ref<Shader>> program = {};
        program.reserve(material.Shaders.size());
        for (const auto& shader : material.Shaders)
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

        renderer->ResourceCache.emplace(material.Id, resource);
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

            if (renderer->ResourceCache.contains(texture->Id))
            {
                continue;
            }

            auto resource = renderer->ResourceFactory->Create(texture);
            TBX_ASSERT(resource, "Rendering: Unable to cache textures for active graphics api.");
            if (!resource)
            {
                continue;
            }

            renderer->ResourceCache.emplace(texture->Id, resource);
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

            if (renderer->ResourceCache.contains(mesh->Id))
            {
                continue;
            }

            auto resource = renderer->ResourceFactory->Create(mesh);
            TBX_ASSERT(resource, "Rendering: Unable to cache meshes for active graphics api.");
            if (!resource)
            {
                continue;
            }

            renderer->ResourceCache.emplace(mesh->Id, resource);
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

