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
    using WindowRendererBindingMap =
        std::unordered_map<Ref<Window>, Ref<IRenderer>, RefHasher<Window>, RefEqual<Window>>;

    /// <summary>
    /// Coordinates render targets, windows, and stage composition for a frame.
    /// </summary>
    class TBX_EXPORT RenderingPipeline
    {
    public:
        RenderingPipeline(
            const std::vector<Ref<IRendererFactory>>& rendererFactories,
            const std::vector<Ref<IGraphicsContextProvider>>& graphicsContextProviders,
            Ref<EventBus> eventBus);
        ~RenderingPipeline();

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

        Ref<IRenderer> CreateRenderer(Ref<IGraphicsContext> context);
        Ref<IGraphicsContext> CreateContext(Ref<Window> window, GraphicsApi api);
        void RecreateRenderersForCurrentApi();

    private:
        std::vector<Ref<Stage>> _openStages = {};
        std::vector<Ref<Stage>> _pendingUploadStages = {};

        WindowRendererBindingMap _windowBindings = {};
        std::unordered_map<GraphicsApi, std::vector<Ref<IRendererFactory>>> _renderFactories = {};
        std::unordered_map<GraphicsApi, std::vector<Ref<IGraphicsContextProvider>>> _configProviders = {};

        Ref<EventBus> _eventBus = nullptr;
        EventListener _eventListener = {};

        GraphicsApi _currGraphicsApi = GraphicsApi::OpenGL;
        RgbaColor _clearColor = {};
    };
}

