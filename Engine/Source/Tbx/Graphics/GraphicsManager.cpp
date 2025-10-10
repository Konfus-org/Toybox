#include "Tbx/PCH.h"
#include "Tbx/Graphics/GraphicsManager.h"
#include "Tbx/Debug/Asserts.h"
#include "Tbx/Windowing/Window.h"
#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

namespace Tbx
{
    GraphicsManager::GraphicsManager(
        GraphicsApi startingApi,
        const std::vector<Ref<IGraphicsBackend>>& backends,
        const std::vector<Ref<IGraphicsContextProvider>>& contextProviders,
        Ref<EventBus> eventBus)
        : _eventBus(eventBus)
        , _eventListener(eventBus)
        , _activeGraphicsApi(startingApi)
    {
        TBX_ASSERT(!backends.empty(), "GraphicsManager: requires at least one graphics backend instance.");
        TBX_ASSERT(_eventBus, "GraphicsManager: requires a valid event bus instance.");

        InitializeRenderers(backends, contextProviders);
        TBX_ASSERT(!_renderers.empty(), "GraphicsManager: No compatible renderer implementations were provided for the available graphics APIs.");

        _pipeline = MakeExclusive<GraphicsPipeline>(CreateDefaultRenderPasses());
        RecreateRenderersForCurrentApi();

        _eventListener.Listen(this, &GraphicsManager::OnWindowOpened);
        _eventListener.Listen(this, &GraphicsManager::OnWindowClosed);
        _eventListener.Listen(this, &GraphicsManager::OnAppSettingsChanged);
        _eventListener.Listen(this, &GraphicsManager::OnStageOpened);
        _eventListener.Listen(this, &GraphicsManager::OnStageClosed);
    }

    void GraphicsManager::Update()
    {
        if (!_pipeline)
        {
            return;
        }

        auto* renderer = GetRenderer(_activeGraphicsApi);
        if (!renderer)
        {
            return;
        }

        _pipeline->SetRenderer(renderer);

        for (const auto& display : _openDisplays)
        {
            TBX_ASSERT(display.Surface && display.Context, "GraphicsManager: Display is missing a surface or context.");
            _pipeline->Render(display, _openStages, _clearColor);
        }

        if (_eventBus)
        {
            _eventBus->Send(RenderedFrameEvent());
        }
    }

    void GraphicsManager::SetRenderPasses(std::vector<RenderPassDescriptor> passes)
    {
        if (passes.empty())
        {
            TBX_ASSERT(false, "GraphicsManager: Render passes must not be empty.");
            return;
        }

        _pipeline->SetRenderPasses(std::move(passes));
    }

    const std::vector<RenderPassDescriptor>& GraphicsManager::GetRenderPasses() const
    {
        return _pipeline->GetRenderPasses();
    }

    void GraphicsManager::InitializeRenderers(
        const std::vector<Ref<IGraphicsBackend>>& backends,
        const std::vector<Ref<IGraphicsContextProvider>>& contextProviders)
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

    GraphicsRenderer* GraphicsManager::GetRenderer(GraphicsApi api)
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
            _pipeline->SetRenderer(nullptr);
            return;
        }

        _pipeline->SetRenderer(renderer);

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
        const auto& newSettings = e.GetNewSettings();
        auto desiredApi = newSettings.RenderingApi;

        if (_activeGraphicsApi != desiredApi)
        {
            _activeGraphicsApi = desiredApi;
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
        auto newWindow = e.GetWindow();
        if (!newWindow)
        {
            TBX_ASSERT(newWindow, "GraphicsManager: Invalid window open event, ensure a valid window is passed!");
            return;
        }

        auto* renderer = GetRenderer(_activeGraphicsApi);
        if (!renderer)
        {
            return;
        }

        auto context = renderer->ContextProvider->Provide(newWindow);
        TBX_ASSERT(context, "GraphicsManager: Failed to create a graphics context for the opened window.");
        if (!context)
        {
            return;
        }

        context->SetVsync(_vsync);

        GraphicsDisplay display = {};
        display.Surface = newWindow;
        display.Context = context;
        _openDisplays.push_back(std::move(display));
    }

    void GraphicsManager::OnWindowClosed(const WindowClosedEvent& e)
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
            TBX_ASSERT(false, "GraphicsManager: A renderer could not be found for the window that was closed!");
        }
    }

    void GraphicsManager::OnStageOpened(const StageOpenedEvent& e)
    {
        auto stage = e.GetStage();
        if (!stage)
        {
            return;
        }

        auto it = std::find(_openStages.begin(), _openStages.end(), stage);
        if (it == _openStages.end())
        {
            _openStages.push_back(stage);
        }
    }

    void GraphicsManager::OnStageClosed(const StageClosedEvent& e)
    {
        auto stage = e.GetStage();
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

    std::vector<RenderPassDescriptor> GraphicsManager::CreateDefaultRenderPasses()
    {
        constexpr std::string_view OpaquePassName = "Opaque";
        constexpr std::string_view TransparentPassName = "Transparent";

        std::vector<RenderPassDescriptor> passes = {};
        passes.reserve(2);

        RenderPassDescriptor opaque = {};
        opaque.Name = std::string(OpaquePassName);
        opaque.DepthTestEnabled = true;
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
        passes.push_back(std::move(opaque));

        RenderPassDescriptor transparent = {};
        transparent.Name = std::string(TransparentPassName);
        transparent.DepthTestEnabled = false;
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
        passes.push_back(std::move(transparent));

        return passes;
    }
}
