#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/GraphicsBackend.h"
#include "Tbx/Graphics/GraphicsContext.h"
#include "Tbx/Graphics/GraphicsResources.h"
#include "Tbx/Graphics/RenderPass.h"
#include "Tbx/Graphics/Frustum.h"
#include "Tbx/Graphics/Material.h"
#include "Tbx/Windowing/Window.h"
#include "Tbx/Stages/Stage.h"
#include "Tbx/Stages/Views.h"
#include "Tbx/Memory/Refs.h"
#include <cstddef>
#include <string>
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
        std::vector<RenderBuckets> PassBuckets = {};
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
        explicit GraphicsPipeline(std::vector<RenderPassDescriptor> passes = {});

        GraphicsPipeline(const GraphicsPipeline&) = delete;
        GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

        GraphicsPipeline(GraphicsPipeline&&) = default;
        GraphicsPipeline& operator=(GraphicsPipeline&&) = default;

        void SetRenderPasses(std::vector<RenderPassDescriptor> passes);
        const std::vector<RenderPassDescriptor>& GetRenderPasses() const { return _renderPasses; }

        void SetRenderer(GraphicsRenderer* renderer);
        void Render(const GraphicsDisplay& display, const std::vector<Ref<Stage>>& stages, const RgbaColor& clearColor);

    private:
        StageRenderData PrepareStageForRendering(const FullStageView& stageView, float aspectRatio);

        bool ShouldCull(const Ref<Toy>& toy, const Frustum& frustum);

        void CacheShaders(const ShaderProgram& shaders);
        void CacheMaterial(const Material& shaders);
        void CacheMesh(const Mesh& mesh);

        size_t ResolveRenderPassIndex(const Material& material) const;

    private:
        std::vector<RenderPassDescriptor> _renderPasses = {};
        GraphicsRenderer* _renderer = nullptr;
    };
}

