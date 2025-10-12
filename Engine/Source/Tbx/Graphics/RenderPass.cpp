#include "Tbx/PCH.h"
#include "Tbx/Graphics/RenderPass.h"
#include "Tbx/Graphics/GraphicsPipeline.h"

namespace Tbx
{
    void RenderPass::DefaultDraw(
        GraphicsPipeline& pipeline,
        const RenderPass& pass,
        GraphicsRenderer& renderer,
        StageRenderData& renderData)
    {
        renderer.Backend->EnableDepthTesting(true);
        pipeline.RenderStage(pass, renderer, renderData);
    }

    RenderPassDraw RenderPass::CreateDefaultDraw(bool enableDepthTesting)
    {
        if (enableDepthTesting)
        {
            return RenderPass::DefaultDraw;
        }

        return [](GraphicsPipeline& pipeline, const RenderPass& pass, GraphicsRenderer& renderer, StageRenderData& renderData)
        {
            renderer.Backend->EnableDepthTesting(false);
            pipeline.RenderStage(pass, renderer, renderData);
            renderer.Backend->EnableDepthTesting(true);
        };
    }
}
