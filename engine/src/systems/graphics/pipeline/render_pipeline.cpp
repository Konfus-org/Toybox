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
        const auto backend = _backend.lock();
        if (!backend)
            return Result(false, "Render pipeline: graphics backend is unavailable.");

        for (const auto& operation : _operations)
        {
            if (token.is_cancelled())
                return Result(false, "Render pipeline execution cancelled.");

            const auto result = operation->execute(*backend, frame_data, token);
            if (!result)
                return make_failure_result("execute", operation->get_debug_info(), result);
        }

        return {};
    }

    Result RenderPipeline::prepare(FrameData& frame_data, const CancellationToken& token)
    {
        for (const auto& operation : _operations)
        {
            if (token.is_cancelled())
                return Result(false, "Render pipeline preparation cancelled.");

            const auto result = operation->prepare(frame_data);
            if (!result)
                return make_failure_result("prepare", operation->get_debug_info(), result);
        }

        return {};
    }

    Result RenderPipeline::prepare(std::unique_ptr<FrameData> frame_data)
    {
        if (!frame_data)
            return Result(false, "Render pipeline: frame data is unavailable.");

        const auto result = prepare(*frame_data, CancellationToken {});
        if (!result)
            return result;

        _prepared_frame_data = std::move(frame_data);
        return {};
    }

    Result RenderPipeline::execute(const CancellationToken& token) const
    {
        if (!_prepared_frame_data)
            return Result(false, "Render pipeline: no prepared frame data is available.");

        return execute(*_prepared_frame_data, token);
    }

    Result RenderPipeline::run(FrameData& frame_data, const CancellationToken& token)
    {
        if (const auto result = prepare(frame_data, token); !result)
            return result;

        return execute(frame_data, token);
    }

    Result RenderPipeline::make_failure_result(
        const char* phase,
        const RenderOperationDebugInfo& debug_info,
        const Result& result) const
    {
        return Result(
            false,
            std::string("Render pipeline ") + phase + " failed in " + debug_info.category + "/"
                + debug_info.debug_name + ": " + result.get_report());
    }
}
