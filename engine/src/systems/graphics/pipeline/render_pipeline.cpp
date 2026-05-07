#include "tbx/systems/graphics/pipeline/render_pipeline.h"
#include <any>
#include <memory>
#include <utility>

namespace tbx
{
    GraphicsRenderPipeline::GraphicsRenderPipeline(IGraphicsBackend& backend)
        : _backend(backend)
    {
    }

    void GraphicsRenderPipeline::add_pass_operation(GraphicsRenderPass pass)
    {
        add_operation(std::make_unique<GraphicsRenderPassOperation>(std::move(pass)));
    }

    void GraphicsRenderPipeline::clear()
    {
        clear_operations();
    }

    Result GraphicsRenderPipeline::execute() const
    {
        return execute(CancellationToken {});
    }

    Result GraphicsRenderPipeline::execute(const CancellationToken& cancellation_token) const
    {
        const auto payload = std::any(
            GraphicsPipelinePayload {
                .backend = std::ref(_backend),
            });
        return Pipeline::execute(payload, cancellation_token);
    }

    IGraphicsBackend& GraphicsRenderPipeline::get_backend() const
    {
        return _backend;
    }

    Result GraphicsRenderPipeline::execute(
        const std::any& payload,
        const CancellationToken& cancellation_token)
    {
        return Pipeline::execute(payload, cancellation_token);
    }
}
