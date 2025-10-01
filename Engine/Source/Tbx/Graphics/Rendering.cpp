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
        for (const auto& factory : rendererFactories)
        {
            if (!factory)
            {
                continue;
            }

            _rendererFactoryCache[GraphicsApi::None].push_back(factory);
        }

        for (const auto& provider : graphicsContextProviders)
        {
            if (!provider)
            {
                continue;
            }

            _contextProviderCache[GraphicsApi::None].push_back(provider);
        }

        const auto hasRendererFactories = std::any_of(
            _rendererFactoryCache.begin(),
            _rendererFactoryCache.end(),
            [](const auto& entry)
            {
                return !entry.second.empty();
            });
        const auto hasContextProviders = std::any_of(
            _contextProviderCache.begin(),
            _contextProviderCache.end(),
            [](const auto& entry)
            {
                return !entry.second.empty();
            });

        TBX_ASSERT(hasRendererFactories, "Rendering: requires at least one renderer factory instance.");
        TBX_ASSERT(hasContextProviders, "Rendering: requires at least one graphics context provider instance.");
        TBX_ASSERT(_eventBus, "Rendering: requires a valid event bus instance.");

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

        const auto* binding = FindBinding(window);
        if (!binding)
        {
            TBX_ASSERT(false, "Rendering: No renderer found for window");
            return nullptr;
        }

        return binding->Renderer;
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
            for (auto& [windowRef, binding] : _windowBindings)
            {
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
        for (auto& [windowRef, binding] : _windowBindings)
        {
            const auto& renderer = binding.Renderer;
            const auto& rendererWindow = windowRef;
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

        auto newConfig = GetConfig(newWindow, _graphicsApi);
        auto newRenderer = CreateRenderer(_graphicsApi);

        _windowBindings[newWindow] = { newConfig, newRenderer };
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
        for (auto it = _rendererFactoryCache.begin(); it != _rendererFactoryCache.end();)
        {
            if (it->first == GraphicsApi::None)
            {
                ++it;
                continue;
            }

            it = _rendererFactoryCache.erase(it);
        }

        for (auto it = _contextProviderCache.begin(); it != _contextProviderCache.end();)
        {
            if (it->first == GraphicsApi::None)
            {
                ++it;
                continue;
            }

            it = _contextProviderCache.erase(it);
        }

        WindowBindingMap newBindings = {};
        newBindings.reserve(_windowBindings.size());

        for (const auto& [windowRef, binding] : _windowBindings)
        {
            const auto& window = windowRef;
            if (!window)
            {
                continue;
            }

            auto config = GetConfig(window, _graphicsApi);
            auto renderer = CreateRenderer(_graphicsApi);

            newBindings.emplace(windowRef, RenderingContext{ config, renderer });
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
        auto cachedFactoryIt = _rendererFactoryCache.find(api);
        if (cachedFactoryIt != _rendererFactoryCache.end())
        {
            for (const auto& cachedFactory : cachedFactoryIt->second)
            {
                if (!cachedFactory)
                {
                    continue;
                }

                auto renderer = cachedFactory->Create();
                if (renderer && renderer->GetApi() == api)
                {
                    return renderer;
                }
            }
        }

        auto fallbackIt = _rendererFactoryCache.find(GraphicsApi::None);
        if (fallbackIt != _rendererFactoryCache.end())
        {
            for (const auto& factory : fallbackIt->second)
            {
                if (!factory)
                {
                    continue;
                }

                auto renderer = factory->Create();
                if (!renderer)
                {
                    continue;
                }

                if (renderer->GetApi() != api)
                {
                    continue;
                }

                auto& cache = _rendererFactoryCache[api];
                if (std::find(cache.begin(), cache.end(), factory) == cache.end())
                {
                    cache.push_back(factory);
                }

                return renderer;
            }
        }

        TBX_ASSERT(false, "Rendering: Unable to locate a renderer factory for requested graphics API.");
        return nullptr;
    }

    Ref<IGraphicsConfig> Rendering::GetConfig(const Ref<Window>& window, GraphicsApi api)
    {
        if (!window)
        {
            return nullptr;
        }

        auto cachedProviderIt = _contextProviderCache.find(api);
        if (cachedProviderIt != _contextProviderCache.end())
        {
            for (const auto& cachedProvider : cachedProviderIt->second)
            {
                if (!cachedProvider)
                {
                    continue;
                }

                auto config = cachedProvider->Create(window, api);
                if (config && config->GetApi() == api)
                {
                    return config;
                }
            }
        }

        auto fallbackIt = _contextProviderCache.find(GraphicsApi::None);
        if (fallbackIt != _contextProviderCache.end())
        {
            for (const auto& provider : fallbackIt->second)
            {
                if (!provider)
                {
                    continue;
                }

                auto config = provider->Create(window, api);
                if (!config)
                {
                    continue;
                }

                if (config->GetApi() != api)
                {
                    continue;
                }

                auto& cache = _contextProviderCache[api];
                if (std::find(cache.begin(), cache.end(), provider) == cache.end())
                {
                    cache.push_back(provider);
                }

                return config;
            }
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

    RenderingContext* Rendering::FindBinding(const Ref<Window>& window)
    {
        if (!window)
        {
            return nullptr;
        }

        auto it = _windowBindings.find(window);
        if (it == _windowBindings.end())
        {
            return nullptr;
        }

        return &it->second;
    }

    const RenderingContext* Rendering::FindBinding(const Ref<Window>& window) const
    {
        if (!window)
        {
            return nullptr;
        }

        auto it = _windowBindings.find(window);
        if (it == _windowBindings.end())
        {
            return nullptr;
        }

        return &it->second;
    }
}

