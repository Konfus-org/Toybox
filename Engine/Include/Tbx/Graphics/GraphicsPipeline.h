#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/GraphicsBackend.h"
#include "Tbx/Graphics/GraphicsContext.h"
#include "Tbx/Graphics/GraphicsResources.h"
#include "Tbx/Graphics/Frustum.h"
#include "Tbx/Graphics/Material.h"
#include "Tbx/Windowing/Window.h"
#include "Tbx/Stages/Stage.h"
#include "Tbx/Stages/Views.h"
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
    using RenderBucket = std::vector<Ref<Toy>>;
    using RenderBuckets = std::unordered_map<Uid, RenderBucket>;

    struct CameraData
    {
        Mat4x4 ViewProjection;
        Frustum Frustum;
    };

    struct StageRenderData
    {
        RenderBuckets Buckets = {};
        std::vector<CameraData> Cameras = {};
    };

    struct TBX_EXPORT GraphicsDisplay
    {
        Ref<Window> Surface = nullptr;
        Ref<IGraphicsContext> Context = nullptr;
    };

    struct TBX_EXPORT GraphicsResourceCache
    {
        // Shader id to shader GPU resource
        std::unordered_map<Uid, Ref<ShaderResource>> Shaders = {};
        // Shader program id to shader program GPU resource
        std::unordered_map<Uid, Ref<ShaderProgramResource>> ShaderPrograms = {};
        // Texture id to texture GPU resource
        std::unordered_map<Uid, Ref<TextureResource>> Textures = {};
        // Mesh id to mesh GPU resource
        std::unordered_map<Uid, Ref<MeshResource>> Meshes = {};

        void Clear()
        {
            Shaders.clear();
            Textures.clear();
            Meshes.clear();
        }
    };

    struct TBX_EXPORT GraphicsRenderer
    {
        Ref<IGraphicsBackend> Backend = nullptr;
        Ref<IGraphicsContextProvider> ContextProvider = nullptr;
        GraphicsResourceCache Cache = {};
    };

    // TODO: Convert to a plugin! The pipeline should be customizable by users of Toybox
    /// <summary>
    /// Coordinates render targets, windows, stage composition, and render resource caching for a frame.
    /// </summary>
    class TBX_EXPORT GraphicsPipeline
    {
    public:
        GraphicsPipeline() = default;
        GraphicsPipeline(
            GraphicsApi startingApi,
            const std::vector<Ref<IGraphicsBackend>>& backends,
            const std::vector<Ref<IGraphicsContextProvider>>& contextProviders,
            Ref<EventBus> eventBus);

        GraphicsPipeline(const GraphicsPipeline&) = delete;
        GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

        GraphicsPipeline(GraphicsPipeline&&) = default;
        GraphicsPipeline& operator=(GraphicsPipeline&&) = default;

        void Update();

    private:
        void DrawFrame();
        StageRenderData PrepareStageForRendering(GraphicsRenderer& renderer, const FullStageView& stageView, float aspectRatio);
        void RenderOpenStages(GraphicsRenderer& renderer);

        bool ShouldCull(const Ref<Toy>& toy, const Frustum& frustum);
        void AddStage(const Ref<Stage>& stage);
        void RemoveStage(const Ref<Stage>& stage);

        void InitializeRenderers(
            const std::vector<Tbx::Ref<Tbx::IGraphicsBackend>>& backends,
            const std::vector<Tbx::Ref<Tbx::IGraphicsContextProvider>>& contextProviders);
        GraphicsRenderer* GetRenderer(GraphicsApi api);
        void RecreateRenderersForCurrentApi();

        void CacheShaders(GraphicsRenderer& renderer, const ShaderProgram& shaders);
        void CacheMaterial(GraphicsRenderer& renderer, const Material& shaders);
        void CacheMesh(GraphicsRenderer& renderer, const Mesh& mesh);

        // TODO: Move some of this to the graphics manager!
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
        VsyncMode _vsync = VsyncMode::Adaptive;
        RgbaColor _clearColor = {};
    };
}

