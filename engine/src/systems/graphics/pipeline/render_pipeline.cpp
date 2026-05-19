#include "tbx/systems/graphics/pipeline/render_pipeline.h"
#include <string>
#include <utility>

namespace tbx
{
    RenderPipeline::RenderPipeline(std::weak_ptr<IGraphicsBackend> backend)
        : _backend(std::move(backend))
    {
    }

    void RenderPipeline::add_operation(std::unique_ptr<IRenderOperation> operation)
    {
        if (!operation)
            return;

        _operations.push_back(std::move(operation));
    }

    void RenderPipeline::clear()
    {
        _operations.clear();
    }

    Result RenderPipeline::execute(FrameData& frame_data, const CancellationToken& token) const
    {
        return {};
    }

    Result RenderPipeline::prepare(FrameData& frame_data, const CancellationToken& token)
    {
        return {};
    }

    Result RenderPipeline::run(FrameData& frame_data, const CancellationToken& token)
    {
        if (const auto result = prepare(frame_data); !result)
            return result;

        return execute(frame_data, token);
    }
}
