#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/GraphicsBackend.h"
#include "Tbx/Graphics/GraphicsContext.h"
#include "Tbx/Graphics/GraphicsResource.h"
#include "Tbx/Graphics/Frustum.h"
#include "Tbx/Graphics/Material.h"
#include "Tbx/Windowing/Window.h"
#include "Tbx/Stages/Stage.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/EventListener.h"
#include "Tbx/Events/StageEvents.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Events/AppEvents.h"
#include "Tbx/Memory/Refs.h"
#include <unordered_map>
#include <vector>

namespace Tbx
{
    struct GraphicsDisplay
    {
        Ref<Window> Surface = nullptr;
        Ref<IGraphicsContext> Context = nullptr;
    };

    struct GraphicsResourceCache
    {
        std::unordered_map<Uid, Ref<ShaderResource>> Shaders = {};
        std::unordered_map<Uid, Ref<TextureResource>> Textures = {};
        std::unordered_map<Uid, Ref<MeshResource>> Meshes = {};

        void Clear()
        {
            Shaders.clear();
            Textures.clear();
            Meshes.clear();
        }
    };

    struct GraphicsRenderer
    {
        Ref<IGraphicsBackend> Backend = nullptr;
        Ref<IGraphicsContextProvider> ContextProvider = nullptr;
        GraphicsResourceCache Cache = {};
    };

    /// <summary>
    /// Coordinates render targets, windows, stage composition, and render resource caching for a frame.
    /// </summary>
    class TBX_EXPORT GraphicsPipeline
    {
    public:
        GraphicsPipeline(
            const std::vector<Ref<IGraphicsBackend>>& backends,
            const std::vector<Ref<IGraphicsContextProvider>>& contextProviders,
            Ref<EventBus> eventBus);
        ~GraphicsPipeline();

        /// <summary>
        /// Drives the rendering pipeline for all open stages and windows.
        /// </summary>
        void Update();

    private:
        void DrawFrame();

        /// <summary>
        /// Iterates active displays and stages to prepare resources before presenting each surface.
        /// </summary>
        void RenderOpenStages(GraphicsRenderer& renderer);

        /// <summary>
        /// Checks whether a toy lies outside all active view frustums and should be skipped.
        /// </summary>
        bool ShouldCull(const Ref<Toy>& toy, const std::vector<Frustum>& frustums);
        void AddStage(const Ref<Stage>& stage);
        void RemoveStage(const Ref<Stage>& stage);

        GraphicsRenderer* TryGetRenderer(GraphicsApi api);
        void RecreateRenderersForCurrentApi();

        void CompileShaders(const std::vector<Ref<Shader>>& shaders);
        void CacheShaders(GraphicsRenderer& renderer, const Material& material);
        void CacheTextures(GraphicsRenderer& renderer, const std::vector<Ref<Texture>>& textures);
        void CacheMeshes(GraphicsRenderer& renderer, const std::vector<Ref<Mesh>>& meshes);

        void OnAppSettingsChanged(const AppSettingsChangedEvent& e);
        void OnWindowOpened(const WindowOpenedEvent& e);
        void OnWindowClosed(const WindowClosedEvent& e);
        void OnStageOpened(const StageOpenedEvent& e);
        void OnStageClosed(const StageClosedEvent& e);

    private:
        std::vector<Ref<Stage>> _openStages = {};
        std::vector<GraphicsDisplay> _openDisplays = {};
        std::unordered_map<GraphicsApi, GraphicsRenderer> _renderers = {};

        Ref<EventBus> _eventBus = nullptr;
        EventListener _eventListener = {};

        GraphicsApi _activeGraphicsApi = GraphicsApi::None;
        RgbaColor _clearColor = {};
    };
}

