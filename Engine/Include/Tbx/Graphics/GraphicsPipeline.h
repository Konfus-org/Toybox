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

    /// <summary>
    /// Coordinates render targets, windows, stage composition, and render resource caching for a frame.
    /// </summary>
    class TBX_EXPORT GraphicsPipeline
    {
    public:
        GraphicsPipeline() = default;
        explicit GraphicsPipeline(std::vector<RenderPass> passes);
        void Render(GraphicsRenderer& renderer, const GraphicsDisplay& display, const std::vector<Ref<Stage>>& stages, const RgbaColor& clearColor);
        void RenderStage(const RenderPass& pass, Tbx::GraphicsRenderer& renderer, Tbx::StageRenderData& renderData);

    private:
        StageRenderData PrepareStageForRendering(GraphicsRenderer& renderer, const FullStageView& stageView, float aspectRatio);
        void RenderCameraView(const Tbx::RenderBucket& bucket, const Tbx::CameraData& camera, const Tbx::Ref<Tbx::ShaderProgramResource>& shaderResource, Tbx::GraphicsRenderer& renderer);
        void CacheShaders(GraphicsRenderer& renderer, const ShaderProgram& shaders);
        void CacheMaterial(GraphicsRenderer& renderer, const Material& shaders);
        void CacheMesh(GraphicsRenderer& renderer, const Mesh& mesh);
        bool ShouldCull(const Ref<Toy>& toy, const Frustum& frustum);
        size_t ResolveRenderPassIndex(const Material& material) const;

    public:
        std::vector<RenderPass> RenderPasses = {};
    };
}

