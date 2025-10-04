#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/GraphicsContext.h"
#include "Tbx/Graphics/GraphicsUploader.h"
#include "Tbx/Graphics/Renderer.h"
#include "Tbx/Graphics/Color.h"
#include "Tbx/Windowing/Window.h"
#include "Tbx/Stages/Stage.h"
#include "Tbx/Events/AppEvents.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/EventListener.h"
#include "Tbx/Events/StageEvents.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Memory/Refs.h"
#include <unordered_map>
#include <vector>

namespace Tbx
{
    /// <summary>
    /// Coordinates render targets, windows, and stage composition for a frame.
    /// </summary>
    class TBX_EXPORT RenderingPipeline
    {
    public:
        RenderingPipeline(
            const std::vector<Ref<IRendererFactory>>& rendererFactories,
            const std::vector<Ref<IGraphicsContextProvider>>& graphicsContextProviders,
            const std::vector<Ref<IShaderCompiler>>& shaderCompilers,
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

        void RenderOpenStages();
        void AddStage(const Ref<Stage>& stage);
        void RemoveStage(const Ref<Stage>& stage);

        Ref<IRenderer> CreateRenderer(Ref<IGraphicsContext> context);
        Ref<IGraphicsContext> CreateContext(Ref<Window> window, GraphicsApi api);
        void RecreateRenderersForCurrentApi();

        void CacheShaders(const std::vector<Ref<Material>>& materials);
        void CacheTextures(const std::vector<Ref<Texture>>& textures);
        void CacheMeshes(const std::vector<Ref<Mesh>>& meshes);

        void OnWindowOpened(const WindowOpenedEvent& e);
        void OnWindowClosed(const WindowClosedEvent& e);
        void OnAppSettingsChanged(const AppSettingsChangedEvent& e);
        void OnStageOpened(const StageOpenedEvent& e);
        void OnStageClosed(const StageClosedEvent& e);

    private:
        std::vector<Ref<Stage>> _openStages = {};
        std::unordered_map<Ref<Window>, Ref<IRenderer>> _windowBindings = {};

        std::unordered_map<GraphicsApi, Ref<IRendererFactory>> _renderFactories = {};
        std::unordered_map<GraphicsApi, Ref<IGraphicsUploader>> _graphicsUploader = {};
        std::unordered_map<GraphicsApi, Ref<IGraphicsContextProvider>> _configProviders = {};
        std::unordered_map<ShaderLang, Ref<IShaderCompiler>> _shaderCompilers = {};

        std::unordered_map<Uid, std::vector<Ref<GraphicsHandle>>> _shaderCache = {};
        std::unordered_map<Uid, Ref<GraphicsHandle>> _textureCache = {};
        std::unordered_map<Uid, Ref<GraphicsHandle>> _meshCache = {};

        Ref<EventBus> _eventBus = nullptr;
        EventListener _eventListener = {};

        GraphicsApi _currGraphicsApi = GraphicsApi::None;
        RgbaColor _clearColor = {};
    };
}

