#include "Tbx/PCH.h"
#include "Tbx/Graphics/Rendering.h"
#include "Tbx/Events/RenderEvents.h"
#include "Tbx/Graphics/RenderCommands.h"
#include "Tbx/Graphics/Viewport.h"
#include "Tbx/Graphics/Camera.h"
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>

namespace Tbx
{
    Rendering::Rendering(
        const std::vector<Ref<IRendererFactory>>& rendererFactories,
        const std::vector<Ref<IGraphicsContextProvider>>& graphicsContextProviders,
        Ref<EventBus> eventBus)
        : _rendererFactories(rendererFactories)
        , _graphicsContextProviders(graphicsContextProviders)
        , _eventBus(eventBus)
        , _eventListener(eventBus)
    {
        TBX_ASSERT(!_rendererFactories.empty(), "Rendering: requires at least one renderer factory instance.");
        TBX_ASSERT(!_graphicsContextProviders.empty(), "Rendering: requires at least one graphics context provider instance.");
        TBX_ASSERT(_eventBus, "Rendering: requires a valid event bus instance.");

        _eventListener.Listen(this, &Rendering::OnWindowOpened);
        _eventListener.Listen(this, &Rendering::OnWindowClosed);
        _eventListener.Listen(this, &Rendering::OnWindowResized);
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

        auto it = std::find_if(_windowBindings.begin(), _windowBindings.end(),
            [&window](const WindowBinding& binding)
            {
                return binding.Window == window;
            });
        if (it == _windowBindings.end())
        {
            TBX_ASSERT(false, "Rendering: No renderer found for window");
            return nullptr;
        }

        return it->Renderer;
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
            for (auto& binding : _windowBindings)
            {
                const auto& renderer = binding.Renderer;
                if (!renderer)
                {
                    continue;
                }

                const auto& context = binding.Context;
                if (context)
                {
                    context->MakeCurrent();
                }

                renderer->Flush();
                renderer->Process(uploadBuffer);
            }
        }

        _pendingUploadStages.clear();
    }

    void Rendering::ProcessOpenStages()
    {
        RenderCommandBufferBuilder builder = {};
        RenderCommandBuffer renderBuffer = {};
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

            const auto stageRenderBuffer = builder.BuildRenderBuffer(spaceRoot);
            for (const auto& command : stageRenderBuffer.Commands)
            {
                renderBuffer.Commands.push_back(command);
            }
        }

        for (auto& binding : _windowBindings)
        {
            const auto& renderer = binding.Renderer;
            const auto& rendererWindow = binding.Window;
            const auto& context = binding.Context;
            if (!renderer || !rendererWindow)
            {
                continue;
            }

            if (context)
            {
                context->MakeCurrent();
            }

            renderer->Process(renderBuffer);

            if (context)
            {
                context->SwapBuffers();
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

        auto newContext = CreateContext(newWindow, _graphicsApi);
        auto newRenderer = CreateRenderer(_graphicsApi);

        if (newContext)
        {
            newContext->MakeCurrent();
        }

        if (newRenderer)
        {
            RenderCommandBuffer renderBuffer = {};
            auto newViewport = Viewport({ 0, 0 }, newWindow->GetSize());
            renderBuffer.Commands.emplace_back(RenderCommandType::SetViewport, newViewport);
            newRenderer->Process(renderBuffer);
        }

        if (newContext)
        {
            newContext->SwapBuffers();
        }

        _windowBindings.push_back({ newWindow, newContext, newRenderer });
        for (const auto& stage : _openStages)
        {
            QueueStageUpload(stage);
        }
    }

    void Rendering::OnWindowClosed(const WindowClosedEvent& e)
    {
        auto closedWindow = e.GetWindow();
        const auto renderer = GetRenderer(closedWindow);
        if (!renderer)
        {
            return;
        }

        auto it = std::find_if(_windowBindings.begin(), _windowBindings.end(),
            [&closedWindow](const WindowBinding& binding)
            {
                return binding.Window == closedWindow;
            });
        if (it != _windowBindings.end())
        {
            _windowBindings.erase(it);
        }
    }

    void Rendering::OnWindowResized(const WindowResizedEvent& e)
    {
        const auto resizedWindow = e.GetWindow();
        const auto renderer = GetRenderer(resizedWindow);
        if (!renderer)
        {
            return;
        }

        Ref<IGraphicsContext> context = nullptr;
        auto bindingIt = std::find_if(_windowBindings.begin(), _windowBindings.end(),
            [&resizedWindow](const WindowBinding& binding)
            {
                return binding.Window == resizedWindow;
            });
        if (bindingIt != _windowBindings.end())
        {
            context = bindingIt->Context;
        }

        for (const auto& stage : _openStages)
        {
            auto cameraView = StageView<Camera>(stage->GetRoot());
            for (const auto& toy : cameraView)
            {
                auto& camera = toy->Blocks.Get<Camera>();
                camera.SetAspect(CalculateAspectRatioFromSize(resizedWindow->GetSize()));
            }
        }

        RenderCommandBuffer renderBuffer = {};
        renderBuffer.Commands.emplace_back(RenderCommandType::SetViewport, Viewport({ 0, 0 }, resizedWindow->GetSize()));

        if (context)
        {
            context->MakeCurrent();
        }

        renderer->Process(renderBuffer);

        if (context)
        {
            context->SwapBuffers();
        }
    }

    void Rendering::RecreateRenderersForCurrentApi()
    {
        _rendererFactoryCache.clear();
        _contextProviderCache.clear();

        std::vector<WindowBinding> newBindings = {};
        newBindings.reserve(_windowBindings.size());

        for (const auto& binding : _windowBindings)
        {
            const auto& window = binding.Window;
            if (!window)
            {
                newBindings.push_back({ nullptr, nullptr, nullptr });
                continue;
            }

            auto context = CreateContext(window, _graphicsApi);
            auto renderer = CreateRenderer(_graphicsApi);

            if (context)
            {
                context->MakeCurrent();
            }

            if (renderer)
            {
                RenderCommandBuffer renderBuffer = {};
                renderBuffer.Commands.emplace_back(RenderCommandType::SetViewport, Viewport({ 0, 0 }, window->GetSize()));
                renderer->Process(renderBuffer);
            }

            if (context)
            {
                context->SwapBuffers();
            }

            newBindings.push_back({ window, context, renderer });
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
            const auto& cachedFactory = cachedFactoryIt->second;
            if (cachedFactory)
            {
                auto renderer = cachedFactory->Create();
                if (renderer && renderer->GetApi() == api)
                {
                    return renderer;
                }
            }

            _rendererFactoryCache.erase(cachedFactoryIt);
        }

        for (const auto& factory : _rendererFactories)
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

            _rendererFactoryCache[api] = factory;
            return renderer;
        }

        TBX_ASSERT(false, "Rendering: Unable to locate a renderer factory for requested graphics API.");
        return nullptr;
    }

    Ref<IGraphicsContext> Rendering::CreateContext(const Ref<Window>& window, GraphicsApi api)
    {
        if (!window)
        {
            return nullptr;
        }

        auto cachedProviderIt = _contextProviderCache.find(api);
        if (cachedProviderIt != _contextProviderCache.end())
        {
            const auto& cachedProvider = cachedProviderIt->second;
            if (cachedProvider)
            {
                auto context = cachedProvider->Create(window, api);
                if (context && context->GetApi() == api)
                {
                    return context;
                }
            }

            _contextProviderCache.erase(cachedProviderIt);
        }

        for (const auto& provider : _graphicsContextProviders)
        {
            if (!provider)
            {
                continue;
            }

            auto context = provider->Create(window, api);
            if (!context)
            {
                continue;
            }

            if (context->GetApi() != api)
            {
                continue;
            }

            _contextProviderCache[api] = provider;
            return context;
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

