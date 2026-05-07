#include "tbx/systems/graphics/pipeline/render_pipeline.h"
#include "tbx/systems/graphics/pipeline/render_frame_context.h"
#include <string>
#include <utility>

namespace tbx
{
    RenderPipeline::RenderPipeline(IGraphicsBackend& backend)
        : _backend(backend)
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

    Result RenderPipeline::execute(
        std::shared_ptr<RenderFrameContext> context,
        const CancellationToken& token) const
    {
        if (!context)
            return Result(false, "Render pipeline execute failed: context is null.");

        for (const auto& operation : _operations)
        {
            if (token && token.is_cancelled())
                return Result(false, "Render pipeline execution cancelled.");
            if (!operation)
                continue;

            if (const auto result = operation->execute(_backend.get(), token); !result)
                return make_failure_result("execute", operation->get_debug_info(), result);
        }

        return {};
    }

    Result RenderPipeline::make_failure_result(
        const char* phase,
        const RenderOperationDebugInfo& debug_info,
        const Result& result) const
    {
        auto report = result.get_report();
        if (report.empty())
            report = "Operation returned failure without a report.";

        return Result(
            false,
            std::string("Render pipeline ") + phase + " failed [" + debug_info.category + " / "
                + debug_info.debug_name + "]: " + report);
    }

    Result RenderPipeline::prepare(std::shared_ptr<RenderFrameContext> context)
    {
        if (!context)
            return Result(false, "Render pipeline prepare failed: context is null.");

        for (const auto& operation : _operations)
        {
            if (!operation)
                continue;

            if (const auto result = operation->prepare(*context); !result)
                return make_failure_result("prepare", operation->get_debug_info(), result);
        }

        return {};
    }

    void RenderPipeline::release()
    {
        for (auto& operation : _operations)
        {
            if (operation)
                operation->release(_backend.get());
        }

        _operations.clear();
    }
}
