#pragma once
#include "Tbx/DllExport.h"
#include <functional>
#include <string>

namespace Tbx
{
    struct Material;
    struct StageDrawData;
    struct GraphicsRenderer;
    class GraphicsPipeline;

    struct RenderPass;

    using RenderPassFilter = std::function<bool(const Material&)>;
    using RenderPassDraw = std::function<void(
        GraphicsPipeline& pipeline,
        GraphicsRenderer& renderer,
        StageDrawData& renderData,
        const RenderPass& pass)>;

    struct TBX_EXPORT RenderPass
    {
        std::string Name = {};
        RenderPassFilter Filter = nullptr;
        RenderPassDraw Draw = nullptr;
    };
}
