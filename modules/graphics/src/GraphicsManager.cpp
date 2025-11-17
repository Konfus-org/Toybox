#include "Tbx/PCH.h"
#include "Tbx/Graphics/GraphicsManager.h"
#include "Tbx/Events/EventCarrier.h"
#include "Tbx/Events/RenderEvents.h"
#include "Tbx/Debug/Asserts.h"
#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

namespace Tbx
{
    GraphicsManager::GraphicsManager() = default;

    GraphicsManager::GraphicsManager(
        GraphicsApi startingApi,
        const std::vector<Ref<IGraphicsBackend>>& backends,
        const std::vector<Ref<IGraphicsContextProvider>>& contextProviders,
        Ref<EventBus> eventBus)
        : _eventListener(eventBus)
        , _carrier(eventBus)
        , _activeGraphicsApi(startingApi)
    {
        TBX_ASSERT(!backends.empty(), "GraphicsManager: requires at least one graphics backend instance.");
        TBX_ASSERT(eventBus, "GraphicsManager: requires a valid event bus instance.");

        InitializeRenderers(backends, contextProviders);
        TBX_ASSERT(!_renderers.empty(), "GraphicsManager: No compatible renderer implementations were provided for the available graphics APIs.");

        _pipeline = GraphicsPipeline(CreateDefaultRenderPasses());
        RecreateRenderersForCurrentApi();

        _eventListener.Listen<AppSettingsChangedEvent>([this](AppSettingsChangedEvent& event)
        {
            OnAppSettingsChanged(event);
        });
        _eventListener.Listen<WindowOpenedEvent>([this](WindowOpenedEvent& event)
        {
            OnWindowOpened(event);
        });
        _eventListener.Listen<WindowClosedEvent>([this](WindowClosedEvent& event)
        {
            OnWindowClosed(event);
        });
        _eventListener.Listen<StageOpenedEvent>([this](StageOpenedEvent& event)
        {
            OnStageOpened(event);
        });
        _eventListener.Listen<StageClosedEvent>([this](StageClosedEvent& event)
        {
            OnStageClosed(event);
        });
    }

    void GraphicsManager::Update()
    {
        auto* renderer = GetRenderer(_activeGraphicsApi);
        if (!renderer)
        {
            return;
        }

        for (const auto& display : _openDisplays)
        {
            _pipeline.Draw(*renderer, display, _openStages, _clearColor);
        }

        _carrier.Send(RenderedFrameEvent());
    }

    void GraphicsManager::SetRenderPasses(const std::vector<RenderPass>& passes)
    {
        if (passes.empty())
        {
            TBX_ASSERT(false, "GraphicsManager: Render passes must not be empty.");
            return;
        }

        _pipeline.RenderPasses = passes;
    }

    const std::vector<RenderPass>& GraphicsManager::GetRenderPasses() const
    {
        return _pipeline.RenderPasses;
    }

    void GraphicsManager::InitializeRenderers(
        const std::vector<Ref<IGraphicsBackend>>& backends,
        const std::vector<Ref<IGraphicsContextProvider>>& contextProviders)
    {
        for (const auto& backend : backends)
        {
            const auto api = backend->GetApi();
            auto& renderer = _renderers[api];
            if (!renderer.Backend)
            {
                renderer.Backend = backend;
            }
        }

        for (const auto& provider : contextProviders)
        {
            const auto api = provider->GetApi();
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
            const auto& renderer = it->second;
            if (!renderer.Backend || !renderer.ContextProvider)
            {
                it = _renderers.erase(it);
                continue;
            }
            ++it;
        }
    }

    GraphicsRenderer* GraphicsManager::GetRenderer(GraphicsApi api)
    {
        auto rendererIt = _renderers.find(api);
        if (rendererIt == _renderers.end())
        {
            return nullptr;
        }
        return &rendererIt->second;
    }

    void GraphicsManager::RecreateRenderersForCurrentApi()
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
            TBX_ASSERT(context, "GraphicsManager: Failed to recreate a graphics context for an open window.");
            if (!context)
            {
                continue;
            }

            context->SetVsync(_vsync);
            display.Context = context;
        }
    }

    void GraphicsManager::OnAppSettingsChanged(const AppSettingsChangedEvent& e)
    {
        const auto& newSettings = e.NewSettings;

        if (_activeGraphicsApi != newSettings.RenderingApi)
        {
            _activeGraphicsApi = newSettings.RenderingApi;
            RecreateRenderersForCurrentApi();
        }

        _vsync = newSettings.Vsync;
        for (auto& display : _openDisplays)
        {
            if (display.Context)
            {
                display.Context->SetVsync(newSettings.Vsync);
            }
        }

        _clearColor = newSettings.ClearColor;
    }

    void GraphicsManager::OnWindowOpened(const WindowOpenedEvent& e)
    {
        auto newWindow = e.AffectedWindow;
        if (!newWindow)
        {
            TBX_ASSERT(false, "GraphicsManager: Invalid window open event, ensure a valid window is passed!");
            return;
        }

        auto* renderer = GetRenderer(_activeGraphicsApi);
        if (!renderer)
        {
            return;
        }

        auto context = renderer->ContextProvider->Provide(newWindow);
        if (!context)
        {
            TBX_ASSERT(false, "GraphicsManager: Failed to create a graphics context for the opened window.");
            return;
        }

        context->SetVsync(_vsync);
        _openDisplays.emplace_back(newWindow, context);
    }

    void GraphicsManager::OnWindowClosed(const WindowClosedEvent& e)
    {
        auto closedWindow = e.AffectedWindow;
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
    }

    void GraphicsManager::OnStageOpened(const StageOpenedEvent& e)
    {
        auto stage = e.OpenedStage;
        auto it = std::find(_openStages.begin(), _openStages.end(), stage);
        if (it == _openStages.end())
        {
            _openStages.push_back(stage);
        }
    }

    void GraphicsManager::OnStageClosed(const StageClosedEvent& e)
    {
        auto stage = e.ClosedStage;
        auto it = std::find(_openStages.begin(), _openStages.end(), stage);
        if (it != _openStages.end())
        {
            _openStages.erase(it);
        }
    }

    std::vector<RenderPass> GraphicsManager::CreateDefaultRenderPasses()
    {
        constexpr std::string_view OpaquePassName = "Opaque";
        constexpr std::string_view TransparentPassName = "Transparent";

        std::vector<RenderPass> passes = {};
        passes.reserve(2);

        RenderPass opaque = {};
        opaque.Name = std::string(OpaquePassName);
        opaque.Filter = [](const Material& material)
        {
            const auto hasAlpha = std::any_of(
                material.Textures.begin(),
                material.Textures.end(),
                [](const Ref<Texture>& texture)
                {
                    return texture && texture->Format == TextureFormat::RGBA;
                });

            return !hasAlpha;
        };
        opaque.Draw = [](GraphicsPipeline& pipeline, GraphicsRenderer& renderer, StageDrawData& renderData, const RenderPass& pass)
        {
            pipeline.Draw(renderer, renderData, pass);
        };
        passes.push_back(std::move(opaque));

        RenderPass transparent = {};
        transparent.Name = std::string(TransparentPassName);
        transparent.Filter = [](const Material& material)
        {
            return std::any_of(
                material.Textures.begin(),
                material.Textures.end(),
                [](const Ref<Texture>& texture)
                {
                    return texture && texture->Format == TextureFormat::RGBA;
                });
        };
        transparent.Draw = [](GraphicsPipeline& pipeline,  GraphicsRenderer& renderer, StageDrawData& renderData, const RenderPass& pass)
        {
            renderer.Backend->EnableDepthTesting(false);
            pipeline.Draw(renderer, renderData, pass);
            renderer.Backend->EnableDepthTesting(true);
        };
        passes.push_back(std::move(transparent));

        return passes;
    }
}
