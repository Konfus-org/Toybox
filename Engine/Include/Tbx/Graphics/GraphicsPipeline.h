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
    using RenderBuckets = std::unordered_map<Uid, std::vector<Ref<Toy>>>;

    struct CameraData
    {
        Mat4x4 ViewProj;
        Frustum Frust;
    };

    struct ModelDrawData
    {
        const Material* Mat = nullptr;
        const Mesh* Poly = nullptr;
    };

    struct StageDrawData
    {
        std::vector<RenderBuckets> PassBuckets = {};
        std::vector<CameraData> Cameras = {};
        std::unordered_map<Uid, ModelDrawData> Drawables = {};
    };

    struct TBX_EXPORT GraphicsDisplay
    {
        const Window* Surface = nullptr;
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
        GraphicsPipeline(std::vector<RenderPass> passes);

        /// <summary>
        /// Prepare a stage for drawing.
        /// </summary>
        StageDrawData Prepare(GraphicsRenderer& renderer, const FullStageView& stageView, float aspectRatio);

        /// <summary>
        /// Clears the display using the given clear color, then draws the given stages to the display.
        /// </summary>
        void Draw(GraphicsRenderer& renderer, const GraphicsDisplay& display, const std::vector<const Stage*>& stages, const RgbaColor& clearColor);

        /// <summary>
        /// Draws the given render data using the given pass.
        /// Each camera will render the set of toys seperately.
        /// </summary>
        void Draw(GraphicsRenderer& renderer, StageDrawData& renderData, const RenderPass& pass);

        /// <summary>
        /// Draws the given toy from the perspective of the given camera using the given shader.
        /// </summary>
        void Draw(GraphicsRenderer& renderer, const CameraData& camera, const std::vector<Ref<Toy>>& toys, const Ref<ShaderProgramResource>& shaderResource, const StageDrawData& renderData);

        /// <summary>
        /// Determines if the given toy should be culled based on the given frustum and the render data.
        /// </summary>
        bool ShouldCull(const Ref<Toy>& toy, const Frustum& frustum, const StageDrawData& renderData);

    private:
        void CacheShaders(GraphicsRenderer& renderer, const ShaderProgram& shaders);
        void CacheMaterial(GraphicsRenderer& renderer, const Material& shaders);
        void CacheMesh(GraphicsRenderer& renderer, const Mesh& mesh);
        size_t ResolveRenderPassIndex(const Material& material) const;

    public:
        std::vector<RenderPass> RenderPasses = {};
    };
}

