#pragma once
#include "Tbx/DllExport.h"
#include <functional>
#include <string>

namespace Tbx
{
    struct Material;
    struct StageRenderData;
    struct GraphicsRenderer;
    class GraphicsPipeline;

    struct RenderPass;

    using RenderPassFilter = std::function<bool(const Material&)>;
    using RenderPassDraw = std::function<void(
        GraphicsPipeline& pipeline,
        const RenderPass& pass,
        GraphicsRenderer& renderer,
        StageRenderData& renderData)>;

    struct TBX_EXPORT RenderPass
    {
        std::string Name = {};
        RenderPassFilter Filter = nullptr;
        RenderPassDraw Draw = nullptr;

        static void DefaultDraw(
            GraphicsPipeline& pipeline,
            const RenderPass& pass,
            GraphicsRenderer& renderer,
            StageRenderData& renderData);

        static RenderPassDraw CreateDefaultDraw(bool enableDepthTesting);
    };
}
