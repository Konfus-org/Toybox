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

        for (auto manager : apiManagers)
        {
            auto supportedApis = manager->GetSupportedApis();
            for (auto supportedApi : supportedApis)
            {
                // Setup the renderers for each api using resource factories
                // context providers and shader compilers that support 
                // The 'supportedApi'
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
        // TODO: Implement
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

    }

    void GraphicsPipeline::OnWindowClosed(const WindowClosedEvent& e)
    {
        auto closedWindow = e.GetWindow();
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

