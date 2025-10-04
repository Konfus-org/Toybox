#include "Tbx/PCH.h"
#include "Tbx/Graphics/GraphicsPipeline.h"
#include "Tbx/Graphics/Shader.h"
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

                GraphicsRenderer renderer = {};
                renderer.ApiManager = manager;
                renderer.ResourceFactory = resourceFactory;
                renderer.ContextProvider = contextProvider;
                renderer.ShaderCompiler = shaderCompiler;

                _renderers.emplace(supportedApi, std::move(renderer));
            }
        }

        _eventListener.Listen(this, &GraphicsPipeline::OnWindowOpened);
        _eventListener.Listen(this, &GraphicsPipeline::OnWindowClosed);
        _eventListener.Listen(this, &GraphicsPipeline::OnAppSettingsChanged);
        _eventListener.Listen(this, &GraphicsPipeline::OnStageOpened);
        _eventListener.Listen(this, &GraphicsPipeline::OnStageClosed);
    }

    GraphicsPipeline::~GraphicsPipeline() = default;

    void GraphicsPipeline::Update()
    {
        DrawFrame();
    }

    void GraphicsPipeline::DrawFrame()
    {
        RenderOpenStages();
    }

    void GraphicsPipeline::RenderOpenStages()
    {
        for (const auto& display : _openDisplays)
        {
            if (!display.Surface || !display.Context)
            {
                continue;
            }

            display.Context->MakeCurrent();
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

                // Iterate through toys and skip those completely outside the view frustum
                for (const auto& toy : stageView)
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
                }
            }

            display.Context->SwapBuffers();
        }

        if (_eventBus)
        {
            _eventBus->Send(RenderedFrameEvent());
        }
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
            if (renderer.ApiManager)
            {
                renderer.ApiManager->Shutdown();
            }

            renderer.ResourceCache.clear();
        }

        for (auto& display : _openDisplays)
        {
            display.Context = nullptr;
        }

        if (_currGraphicsApi == GraphicsApi::None)
        {
            return;
        }

        auto rendererIt = _renderers.find(_currGraphicsApi);
        if (rendererIt == _renderers.end())
        {
            return;
        }

        auto& renderer = rendererIt->second;
        if (!renderer.ContextProvider)
        {
            return;
        }

        for (auto& display : _openDisplays)
        {
            if (!display.Surface)
            {
                continue;
            }

            display.Context = renderer.ContextProvider->Provide(display.Surface, _currGraphicsApi);
            if (display.Context && renderer.ApiManager)
            {
                renderer.ApiManager->Initialize(display.Context, _currGraphicsApi);
            }
        }
    }

    void GraphicsPipeline::CompileShaders(const std::vector<Ref<Shader>>& shaders)
    {
        if (shaders.empty())
        {
            return;
        }

        auto rendererIt = _renderers.find(_currGraphicsApi);
        if (rendererIt == _renderers.end())
        {
            return;
        }

        auto& renderer = rendererIt->second;
        if (!renderer.ShaderCompiler)
        {
            return;
        }

        for (const auto& shader : shaders)
        {
            if (!shader)
            {
                continue;
            }

            renderer.ShaderCompiler->Compile(shader, _currGraphicsApi);
        }
    }

    void GraphicsPipeline::CacheShaders(const std::vector<Ref<Shader>>& shaders)
    {
        if (shaders.empty())
        {
            return;
        }

        auto rendererIt = _renderers.find(_currGraphicsApi);
        if (rendererIt == _renderers.end())
        {
            return;
        }

        auto& renderer = rendererIt->second;
        if (!renderer.ResourceFactory)
        {
            return;
        }

        for (const auto& shader : shaders)
        {
            if (!shader)
            {
                continue;
            }

            if (renderer.ResourceCache.contains(shader->Id))
            {
                continue;
            }

            auto resource = renderer.ResourceFactory->Create(std::vector<Ref<Shader>>{ shader });
            if (!resource)
            {
                continue;
            }

            resource->RenderId = shader->Id;
            renderer.ResourceCache.emplace(shader->Id, std::move(resource));
        }
    }

    void GraphicsPipeline::CacheTextures(const std::vector<Ref<Texture>>& textures)
    {
        if (textures.empty())
        {
            return;
        }

        auto rendererIt = _renderers.find(_currGraphicsApi);
        if (rendererIt == _renderers.end())
        {
            return;
        }

        auto& renderer = rendererIt->second;
        if (!renderer.ResourceFactory)
        {
            return;
        }

        for (const auto& texture : textures)
        {
            if (!texture)
            {
                continue;
            }

            if (renderer.ResourceCache.contains(texture->Id))
            {
                continue;
            }

            auto resource = renderer.ResourceFactory->Create(texture);
            if (!resource)
            {
                continue;
            }

            resource->RenderId = texture->Id;
            renderer.ResourceCache.emplace(texture->Id, std::move(resource));
        }
    }

    void GraphicsPipeline::CacheMeshes(const std::vector<Ref<Mesh>>& meshes)
    {
        if (meshes.empty())
        {
            return;
        }

        auto rendererIt = _renderers.find(_currGraphicsApi);
        if (rendererIt == _renderers.end())
        {
            return;
        }

        auto& renderer = rendererIt->second;
        if (!renderer.ResourceFactory)
        {
            return;
        }

        for (const auto& mesh : meshes)
        {
            if (!mesh)
            {
                continue;
            }

            if (renderer.ResourceCache.contains(mesh->Id))
            {
                continue;
            }

            auto resource = renderer.ResourceFactory->Create(mesh);
            if (!resource)
            {
                continue;
            }

            resource->RenderId = mesh->Id;
            renderer.ResourceCache.emplace(mesh->Id, std::move(resource));
        }
    }

    void GraphicsPipeline::OnAppSettingsChanged(const AppSettingsChangedEvent& e)
    {
        const auto& newSettings = e.GetNewSettings();
        _clearColor = newSettings.ClearColor;
        const auto previousApi = _currGraphicsApi;
        _currGraphicsApi = newSettings.RenderingApi;

        if (previousApi != _currGraphicsApi)
        {
            RecreateRenderersForCurrentApi();
        }
    }

    void GraphicsPipeline::OnWindowOpened(const WindowOpenedEvent& e)
    {
        auto newWindow = e.GetWindow();
        if (!newWindow || _currGraphicsApi == GraphicsApi::None)
        {
            TBX_ASSERT(_currGraphicsApi != GraphicsApi::None, "Rendering: Invalid graphics API on window open, ensure the api is set before a window is opened!");
            TBX_ASSERT(newWindow, "Rendering: Invalid window open event, ensure a valid window is passed!");
            return;
        }
        auto rendererIt = _renderers.find(_currGraphicsApi);
        if (rendererIt == _renderers.end())
        {
            return;
        }

        auto& renderer = rendererIt->second;
        if (!renderer.ContextProvider)
        {
            return;
        }

        auto displayIt = std::find_if(_openDisplays.begin(), _openDisplays.end(), [newWindow](const GraphicsDisplay& display)
        {
            return display.Surface == newWindow;
        });

        auto context = renderer.ContextProvider->Provide(newWindow, _currGraphicsApi);
        TBX_ASSERT(context, "Rendering: Failed to create a graphics context for the opened window.");
        if (!context)
        {
            return;
        }

        if (renderer.ApiManager)
        {
            renderer.ApiManager->Initialize(context, _currGraphicsApi);
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
            auto rendererIt = _renderers.find(_currGraphicsApi);
            if (rendererIt != _renderers.end() && rendererIt->second.ApiManager)
            {
                rendererIt->second.ApiManager->Shutdown();
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
}

