#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/GraphicsContext.h"
#include "Tbx/Graphics/IRenderer.h"
#include "Tbx/Windowing/Window.h"
#include "Tbx/Stages/Stage.h"
#include "Tbx/Events/AppEvents.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/EventListener.h"
#include "Tbx/Events/TSSEvents.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Graphics/Color.h"
#include <unordered_map>
#include <vector>

namespace Tbx
{
    struct RenderingContext
    {
        Ref<Window> Window = nullptr;
        Ref<IGraphicsConfig> Config = nullptr;
        Ref<IRenderer> Renderer = nullptr;
    };

    using WindowBindingMap = std::unordered_map<Window*, RenderingContext>;

    /// <summary>
    /// Coordinates render targets, windows, and stage composition for a frame.
    /// </summary>
    class TBX_EXPORT Rendering
    {
    public:
        Rendering(
            const std::vector<Ref<IRendererFactory>>& rendererFactories,
            const std::vector<Ref<IGraphicsConfigProvider>>& graphicsContextProviders,
            Ref<EventBus> eventBus);
        ~Rendering();

        /// <summary>
        /// Drives the rendering pipeline for all open stages and windows.
        /// </summary>
        void Update();

        /// <summary>
        /// Retrieves the renderer associated with the provided window.
        /// </summary>
        Ref<IRenderer> GetRenderer(const Ref<Window>& window) const;

    private:
        void DrawFrame();
        void ProcessPendingUploads();
        void ProcessOpenStages();

        void QueueStageUpload(const Ref<Stage>& stage);
        void AddStage(const Ref<Stage>& stage);
        void RemoveStage(const Ref<Stage>& stage);

        void OnWindowOpened(const WindowOpenedEvent& e);
        void OnWindowClosed(const WindowClosedEvent& e);
        void OnAppSettingsChanged(const AppSettingsChangedEvent& e);
        void OnStageOpened(const StageOpenedEvent& e);
        void OnStageClosed(const StageClosedEvent& e);

        Ref<IRenderer> CreateRenderer(GraphicsApi api);
        Ref<IGraphicsConfig> GetConfig(const Ref<Window>& window, GraphicsApi api);
        void RecreateRenderersForCurrentApi();

    private:
        std::vector<Ref<Stage>> _openStages = {};
        WindowBindingMap _windowBindings = {};
        std::vector<Ref<Stage>> _pendingUploadStages = {};
        std::vector<Ref<IRendererFactory>> _rendererFactories = {};
        std::vector<Ref<IGraphicsConfigProvider>> _graphicsConfigProviders = {};
        std::unordered_map<GraphicsApi, Ref<IRendererFactory>> _rendererFactoryCache = {};
        std::unordered_map<GraphicsApi, Ref<IGraphicsConfigProvider>> _configProviderCache = {};
        Ref<EventBus> _eventBus = nullptr;
        EventListener _eventListener = {};
        GraphicsApi _graphicsApi = GraphicsApi::OpenGL;
        RgbaColor _clearColor = {};
    };
}

