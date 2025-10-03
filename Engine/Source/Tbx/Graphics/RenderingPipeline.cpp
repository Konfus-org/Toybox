#include "Tbx/PCH.h"
#include "Tbx/Graphics/RenderingPipeline.h"
#include "Tbx/Events/RenderEvents.h"
#include "Tbx/Graphics/RenderCommands.h"
#include "Tbx/Graphics/Viewport.h"
#include "Tbx/Math/Size.h"
#include <algorithm>
#include <utility>

namespace Tbx
{
    RenderingPipeline::RenderingPipeline(
        const std::vector<Ref<IRendererFactory>>& rendererFactories,
        const std::vector<Ref<IGraphicsContextProvider>>& graphicsContextProviders,
        Ref<EventBus> eventBus)
        : _eventBus(eventBus)
        , _eventListener(eventBus)
    {
        TBX_ASSERT(!rendererFactories.empty(), "Rendering: requires at least one renderer factory instance.");
        TBX_ASSERT(!graphicsContextProviders.empty(), "Rendering: requires at least one graphics context provider instance.");
        TBX_ASSERT(_eventBus, "Rendering: requires a valid event bus instance.");

        for (const auto& factory : rendererFactories)
        {
            if (!factory)
            {
                continue;
            }

            const auto supportedApis = factory->GetSupportedApis();
            if (supportedApis.empty())
            {
                TBX_ASSERT(false, "Rendering: Renderer factory must advertise at least one supported graphics API.");
                continue;
            }

            for (const auto supportedApi : supportedApis)
            {
                auto& factoryBucket = _renderFactories[supportedApi];
                const auto alreadyRegistered = std::find(factoryBucket.begin(), factoryBucket.end(), factory);
                if (alreadyRegistered != factoryBucket.end())
                {
                    continue;
                }

                factoryBucket.push_back(factory);
            }
        }

        for (const auto& provider : graphicsContextProviders)
        {
            if (!provider)
            {
                continue;
            }

            const auto supportedApis = provider->GetSupportedApis();
            if (supportedApis.empty())
            {
                TBX_ASSERT(false, "Rendering: Graphics context provider must advertise at least one supported graphics API.");
                continue;
            }

            for (const auto supportedApi : supportedApis)
            {
                auto& providerBucket = _configProviders[supportedApi];
                const auto alreadyRegistered = std::find(providerBucket.begin(), providerBucket.end(), provider);
                if (alreadyRegistered != providerBucket.end())
                {
                    continue;
                }

                providerBucket.push_back(provider);
            }
        }

        _eventListener.Listen(this, &RenderingPipeline::OnWindowOpened);
        _eventListener.Listen(this, &RenderingPipeline::OnWindowClosed);
        _eventListener.Listen(this, &RenderingPipeline::OnAppSettingsChanged);
        _eventListener.Listen(this, &RenderingPipeline::OnStageOpened);
        _eventListener.Listen(this, &RenderingPipeline::OnStageClosed);
    }

    RenderingPipeline::~RenderingPipeline() = default;

    void RenderingPipeline::Update()
    {
        if (_windowBindings.empty() || _openStages.empty())
        {
            return;
        }

        DrawFrame();
    }

    Ref<IRenderer> RenderingPipeline::GetRenderer(const Ref<Window>& window) const
    {
        if (!window)
        {
            return nullptr;
        }

        auto it = _windowBindings.find(window);
        if (it == _windowBindings.end())
        {
            TBX_ASSERT(false, "Rendering: No renderer found for window");
            return nullptr;
        }

        return it->second;
    }

    void RenderingPipeline::DrawFrame()
    {
        ProcessStageUploads();
        ProcessStageRenders();
    }

    void RenderingPipeline::ProcessStageUploads()
    {
        RenderCommandBufferBuilder builder = {};
        RenderCommandBuffer uploadBuffer = {};
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

            const auto stageUploadBuffer = builder.BuildUploadBuffer(spaceRoot);
            for (const auto& command : stageUploadBuffer.Commands)
            {
                uploadBuffer.Commands.push_back(command);
            }
        }

        if (!uploadBuffer.Commands.empty())
        {
            for (auto& [window, renderer] : _windowBindings)
            {
                renderer->Flush();
                renderer->Process(uploadBuffer);
            }
        }
    }

    void RenderingPipeline::ProcessStageRenders()
    {
        for (auto& [window, renderer] : _windowBindings)
        {
            RenderCommandBufferBuilder builder = {};
            RenderCommandBuffer renderBuffer = {};

            const auto windowSize = window->GetSize();
            const auto aspectRatio = CalculateAspectRatioFromSize(windowSize);
            renderBuffer.Commands.emplace_back(RenderCommandType::SetViewport, Viewport({ 0, 0 }, windowSize));
            renderBuffer.Commands.emplace_back(RenderCommandType::Clear, _clearColor);

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

                const auto stageRenderBuffer = builder.BuildRenderBuffer(spaceRoot, aspectRatio);
                for (const auto& command : stageRenderBuffer.Commands)
                {
                    renderBuffer.Commands.push_back(command);
                }
            }

            renderer->Process(renderBuffer);
        }

        if (_eventBus)
        {
            _eventBus->Send(RenderedFrameEvent());
        }
    }

    void RenderingPipeline::AddStage(const Ref<Stage>& stage)
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

    void RenderingPipeline::RemoveStage(const Ref<Stage>& stage)
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

    void RenderingPipeline::RecreateRenderersForCurrentApi()
    {
        WindowRendererBindingMap newBindings = {};
        newBindings.reserve(_windowBindings.size());

        for (const auto& bindingEntry : _windowBindings)
        {
            const auto& window = bindingEntry.first;
            if (!window)
            {
                continue;
            }

            auto context = CreateContext(window, _currGraphicsApi);
            TBX_ASSERT(context, "Rendering: Unable to recreate graphics context for window.");
            auto renderer = CreateRenderer(context);
            TBX_ASSERT(renderer, "Rendering: Unable to recreate renderer for window.");

            newBindings.emplace(window, renderer);
        }

        _windowBindings = std::move(newBindings);
    }

    Ref<IRenderer> RenderingPipeline::CreateRenderer(Ref<IGraphicsContext> context)
    {
        auto& factories = _renderFactories[context->GetApi()];
        for (auto it = factories.begin(); it != factories.end();)
        {
            Ref<IRendererFactory> factory = *it;
            if (!factory)
            {
                it = factories.erase(it);
                continue;
            }

            auto renderer = factory->Create(context);
            if (!renderer)
            {
                ++it;
                continue;
            }

            const auto rendererApi = renderer->GetApi();
            if (rendererApi != context->GetApi())
            {
                TBX_ASSERT(false, "Rendering: Renderer factory produced mismatched API.");
                it = factories.erase(it);
                continue;
            }

            return renderer;
        }

        TBX_ASSERT(false, "Rendering: Unable to locate a renderer factory for requested graphics API.");
        return nullptr;
    }

    Ref<IGraphicsContext> RenderingPipeline::CreateContext(Ref<Window> window, GraphicsApi api)
    {
        if (!window)
        {
            return nullptr;
        }

        auto& providers = _configProviders[api];
        for (auto it = providers.begin(); it != providers.end();)
        {
            Ref<IGraphicsContextProvider> provider = *it;
            if (!provider)
            {
                it = providers.erase(it);
                continue;
            }

            auto config = provider->Provide(window, api);
            if (!config)
            {
                ++it;
                continue;
            }

            const auto configApi = config->GetApi();
            if (configApi != api)
            {
                TBX_ASSERT(false, "Rendering: Graphics context provider produced mismatched API.");
                it = providers.erase(it);
                continue;
            }

            return config;
        }

        TBX_ASSERT(false, "Rendering: Unable to locate a graphics context provider for requested graphics API.");
        return nullptr;
    }

    void RenderingPipeline::OnWindowOpened(const WindowOpenedEvent& e)
    {
        auto newWindow = e.GetWindow();
        if (!newWindow || _currGraphicsApi == GraphicsApi::None)
        {
            TBX_ASSERT(_currGraphicsApi != GraphicsApi::None, "Rendering: Invalid graphics API on window open, ensure the api is set before a window is opened!");
            TBX_ASSERT(newWindow, "Rendering: Invalid window open event, ensure a valid window is passed!");
            return;
        }

        auto newConfig = CreateContext(newWindow, _currGraphicsApi);
        auto newRenderer = CreateRenderer(newConfig);

        _windowBindings.emplace(newWindow, newRenderer);
    }

    void RenderingPipeline::OnWindowClosed(const WindowClosedEvent& e)
    {
        auto closedWindow = e.GetWindow();
        if (!closedWindow)
        {
            return;
        }

        _windowBindings.erase(closedWindow);
    }

    void RenderingPipeline::OnAppSettingsChanged(const AppSettingsChangedEvent& e)
    {
        const auto& newSettings = e.GetNewSettings();
        _clearColor = newSettings.ClearColor;
        const auto previousApi = _currGraphicsApi;
        _currGraphicsApi = newSettings.Api;

        if (previousApi != _currGraphicsApi)
        {
            RecreateRenderersForCurrentApi();
        }
    }

    void RenderingPipeline::OnStageOpened(const StageOpenedEvent& e)
    {
        AddStage(e.GetStage());
    }

    void RenderingPipeline::OnStageClosed(const StageClosedEvent& e)
    {
        RemoveStage(e.GetStage());
    }
}

