#include "Tbx/PCH.h"
#include "Tbx/Graphics/Rendering.h"
#include "Tbx/Events/RenderEvents.h"
#include "Tbx/Graphics/RenderCommands.h"
#include "Tbx/Graphics/Viewport.h"
#include "Tbx/Graphics/Camera.h"
#include "Tbx/Math/Size.h"
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>

namespace Tbx
{
    Rendering::Rendering(
        const std::vector<Ref<IRendererFactory>>& rendererFactories,
        const std::vector<Ref<IGraphicsConfigProvider>>& graphicsContextProviders,
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
                TBX_ASSERT(false, "Rendering: Graphics config provider must advertise at least one supported graphics API.");
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

        _eventListener.Listen(this, &Rendering::OnWindowOpened);
        _eventListener.Listen(this, &Rendering::OnWindowClosed);
        _eventListener.Listen(this, &Rendering::OnAppSettingsChanged);
        _eventListener.Listen(this, &Rendering::OnStageOpened);
        _eventListener.Listen(this, &Rendering::OnStageClosed);
    }

    Rendering::~Rendering() = default;

    void Rendering::Update()
    {
        if (_windowBindings.empty() || _openStages.empty())
        {
            return;
        }

        DrawFrame();
    }

    Ref<IRenderer> Rendering::GetRenderer(const Ref<Window>& window) const
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

        return it->second.Renderer;
    }

    void Rendering::DrawFrame()
    {
        ProcessPendingUploads();
        ProcessOpenStages();
    }

    void Rendering::QueueStageUpload(const Ref<Stage>& stage)
    {
        if (!stage)
        {
            return;
        }

        const auto it = std::find(_pendingUploadStages.begin(), _pendingUploadStages.end(), stage);
        if (it == _pendingUploadStages.end())
        {
            _pendingUploadStages.push_back(stage);
        }
    }

    void Rendering::ProcessPendingUploads()
    {
        if (_pendingUploadStages.empty())
        {
            return;
        }

        RenderCommandBufferBuilder builder = {};
        RenderCommandBuffer uploadBuffer = {};
        for (const auto& stage : _pendingUploadStages)
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
            for (auto& bindingEntry : _windowBindings)
            {
                auto& binding = bindingEntry.second;
                const auto& renderer = binding.Renderer;
                if (!renderer)
                {
                    continue;
                }

                const auto& config = binding.Config;
                if (config)
                {
                    config->MakeCurrent();
                }

                renderer->Flush();
                renderer->Process(uploadBuffer);
            }
        }

        _pendingUploadStages.clear();
    }

    void Rendering::ProcessOpenStages()
    {
        for (auto& bindingEntry : _windowBindings)
        {
            auto& binding = bindingEntry.second;
            const auto& rendererWindow = bindingEntry.first;
            const auto& renderer = binding.Renderer;
            const auto& config = binding.Config;
            if (!renderer || !rendererWindow)
            {
                continue;
            }

            RenderCommandBufferBuilder builder = {};
            RenderCommandBuffer renderBuffer = {};
            renderBuffer.Commands.emplace_back(RenderCommandType::Clear, _clearColor);

            const auto windowSize = rendererWindow->GetSize();
            renderBuffer.Commands.emplace_back(RenderCommandType::SetViewport, Viewport({ 0, 0 }, windowSize));

            const auto aspectRatio = CalculateAspectRatioFromSize(windowSize);

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

            if (config)
            {
                config->MakeCurrent();
            }

            renderer->Process(renderBuffer);

            if (config)
            {
                config->SwapBuffers();
            }
        }

        if (_eventBus)
        {
            _eventBus->Send(RenderedFrameEvent());
        }
    }

    void Rendering::AddStage(const Ref<Stage>& stage)
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
        QueueStageUpload(stage);
    }

    void Rendering::RemoveStage(const Ref<Stage>& stage)
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

        auto pending = std::find(_pendingUploadStages.begin(), _pendingUploadStages.end(), stage);
        if (pending != _pendingUploadStages.end())
        {
            _pendingUploadStages.erase(pending);
        }
    }

    void Rendering::OnWindowOpened(const WindowOpenedEvent& e)
    {
        auto newWindow = e.GetWindow();
        if (!newWindow)
        {
            return;
        }

        auto newConfig = CreateConfig(newWindow, _graphicsApi);
        auto newRenderer = CreateRenderer(_graphicsApi);

        _windowBindings.emplace(newWindow, RenderingContext{ newConfig, newRenderer });
        for (const auto& stage : _openStages)
        {
            QueueStageUpload(stage);
        }
    }

    void Rendering::OnWindowClosed(const WindowClosedEvent& e)
    {
        auto closedWindow = e.GetWindow();
        if (!closedWindow)
        {
            return;
        }

        _windowBindings.erase(closedWindow);
    }

    void Rendering::RecreateRenderersForCurrentApi()
    {
        WindowBindingMap newBindings = {};
        newBindings.reserve(_windowBindings.size());

        for (const auto& bindingEntry : _windowBindings)
        {
            const auto& window = bindingEntry.first;
            if (!window)
            {
                continue;
            }

            auto config = CreateConfig(window, _graphicsApi);
            TBX_ASSERT(config, "Rendering: Unable to recreate graphics config for window.");
            auto renderer = CreateRenderer(_graphicsApi);
            TBX_ASSERT(renderer, "Rendering: Unable to recreate renderer for window.");

            newBindings.emplace(window, RenderingContext{ config, renderer });
        }

        _windowBindings = std::move(newBindings);

        _pendingUploadStages.clear();
        for (const auto& stage : _openStages)
        {
            QueueStageUpload(stage);
        }
    }

    Ref<IRenderer> Rendering::CreateRenderer(GraphicsApi api)
    {
        auto& factories = _renderFactories[api];
        for (auto it = factories.begin(); it != factories.end();)
        {
            Ref<IRendererFactory> factory = *it;
            if (!factory)
            {
                it = factories.erase(it);
                continue;
            }

            auto renderer = factory->Create(api);
            if (!renderer)
            {
                ++it;
                continue;
            }

            const auto rendererApi = renderer->GetApi();
            if (rendererApi != api)
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

    Ref<IGraphicsConfig> Rendering::CreateConfig(const Ref<Window>& window, GraphicsApi api)
    {
        if (!window)
        {
            return nullptr;
        }

        auto& providers = _configProviders[api];
        for (auto it = providers.begin(); it != providers.end();)
        {
            Ref<IGraphicsConfigProvider> provider = *it;
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
                TBX_ASSERT(false, "Rendering: Graphics config provider produced mismatched API.");
                it = providers.erase(it);
                continue;
            }

            return config;
        }

        TBX_ASSERT(false, "Rendering: Unable to locate a graphics context provider for requested graphics API.");
        return nullptr;
    }

    void Rendering::OnAppSettingsChanged(const AppSettingsChangedEvent& e)
    {
        const auto& newSettings = e.GetNewSettings();
        _clearColor = newSettings.ClearColor;
        const auto previousApi = _graphicsApi;
        _graphicsApi = newSettings.Api;

        if (previousApi != _graphicsApi)
        {
            RecreateRenderersForCurrentApi();
        }
    }

    void Rendering::OnStageOpened(const StageOpenedEvent& e)
    {
        AddStage(e.GetStage());
    }

    void Rendering::OnStageClosed(const StageClosedEvent& e)
    {
        RemoveStage(e.GetStage());
    }
}

