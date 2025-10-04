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
        const std::vector<Ref<IShaderCompiler>>& shaderCompilers,
        Ref<EventBus> eventBus)
        : _eventBus(eventBus)
        , _eventListener(eventBus)
    {
        TBX_ASSERT(!rendererFactories.empty(), "Rendering: requires at least one renderer factory instance.");
        TBX_ASSERT(!graphicsContextProviders.empty(), "Rendering: requires at least one graphics context provider instance.");
        TBX_ASSERT(_eventBus, "Rendering: requires a valid event bus instance.");

        for (const auto& factory : rendererFactories)
        {
            const auto supportedApis = factory->GetSupportedApis();
            if (supportedApis.empty())
            {
                TBX_ASSERT(false, "Rendering: Renderer factory must advertise at least one supported graphics API.");
                continue;
            }

            for (const auto supportedApi : supportedApis)
            {
                _renderFactories[supportedApi] = factory;
            }
        }

        for (const auto& provider : graphicsContextProviders)
        {
            const auto supportedApis = provider->GetSupportedApis();
            if (supportedApis.empty())
            {
                TBX_ASSERT(false, "Rendering: Graphics context provider must advertise at least one supported graphics API.");
                continue;
            }
            for (const auto supportedApi : supportedApis)
            {
                _configProviders[supportedApi] = provider;
            }
        }

        for (const auto& compiler : shaderCompilers)
        {
            const auto supportedLangs = compiler->GetSupportedLanguages();
            if (supportedLangs.empty())
            {
                TBX_ASSERT(false, "Rendering: Shader compiler must advertise at least one supported language.");
                continue;
            }
            for (const auto supportedLang : supportedLangs)
            {
                _shaderCompilers[supportedLang] = compiler;
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
        RenderOpenStages();
    }

    void RenderingPipeline::RenderOpenStages()
    {
        for (auto& [window, renderer] : _windowBindings)
        {
            DrawCommandBufferBuilder builder = {};
            const auto rendBuffer = builder.Build(_openStages, window->GetSize(), _clearColor);
            renderer->Flush();
            renderer->Draw(rendBuffer);
        }

        if (_eventBus)
        {
            _eventBus->Send(RenderedFrameEvent());
        }
    }

    void RenderingPipeline::CacheShaders(const std::vector<Ref<Material>>& materials)
    {
        for (auto material : materials)
        {
            if (_shaderCache.contains(material->Id))
            {
                continue;
            }

            for (auto shader : material->Shaders)
            {
                auto compilersEntry = _shaderCompilers.find(shader->Lang);
                if (compilersEntry == _shaderCompilers.end())
                {
                    TBX_ASSERT(false, "Rendering: No shader compiler found for language.");
                    continue;
                }

                auto compiler = compilersEntry->second;
                compiler->Compile(shader);

                auto uploader = _graphicsUploader[_currGraphicsApi];
                auto handle = uploader->UploadShader(shader);
                _shaderCache[material->Id].push_back(handle);
            }
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
        std::unordered_map<Ref<Window>, Ref<IRenderer>> newBindings = {};
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
        auto& factory = _renderFactories[context->GetApi()];
        auto renderer = factory->Create(context);
        if (!renderer)
        {
            TBX_ASSERT(false, "Rendering: Failed to create renderer for graphics API.");
            return nullptr;
        }
        const auto rendererApi = renderer->GetApi();
        if (rendererApi != context->GetApi())
        {
            TBX_ASSERT(false, "Rendering: Renderer factory produced mismatched API.");
            return nullptr;
        }

        return renderer;
    }

    Ref<IGraphicsContext> RenderingPipeline::CreateContext(Ref<Window> window, GraphicsApi api)
    {
        auto& provider = _configProviders[api];
        auto config = provider->Provide(window, api);
        if (!config)
        {
            TBX_ASSERT(false, "Rendering: Failed to create graphics context for window and selected api.");
            return nullptr;
        }
        const auto configApi = config->GetApi();
        if (configApi != api)
        {
            TBX_ASSERT(false, "Rendering: Graphics context provider produced mismatched API.");
            return nullptr;
        }

        return config;
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
        _currGraphicsApi = newSettings.RenderingApi;

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

