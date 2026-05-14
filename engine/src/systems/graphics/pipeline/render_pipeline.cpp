#include "tbx/systems/graphics/pipeline/render_pipeline.h"
#include "tbx/systems/debugging/macros.h"
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

    Result RenderPipeline::execute(const CancellationToken& token) const
    {
        auto backend = _backend.lock();
        if (!backend)
            return Result(false, "Render pipeline execute failed: graphics backend is unavailable.");

        if (!_render_data)
            return Result(false, "Render pipeline execute failed: render data is null.");

        auto had_recoverable_failures = false;
        auto recoverable_failure_report = std::string {};
        for (const auto& operation : _operations)
        {
            if (token && token.is_cancelled())
                return Result(false, "Render pipeline execution cancelled.");
            if (!operation)
                continue;

            if (const auto result = operation->execute(*backend, *_render_data, token);
                !result)
            {
                had_recoverable_failures = true;
                const auto failure = make_failure_result("execute", operation->get_debug_info(), result);
                if (!recoverable_failure_report.empty())
                    recoverable_failure_report += "\n";
                recoverable_failure_report += failure.get_report();
            }
        }

        if (had_recoverable_failures)
        {
            return Result(
                true,
                "Render pipeline execute completed with recoverable operation failures:\n"
                    + recoverable_failure_report);
        }

        return {};
    }

    RenderData* RenderPipeline::get_render_data() const
    {
        return _render_data.get();
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

    Result RenderPipeline::prepare(std::unique_ptr<RenderData> render_data)
    {
        if (!render_data)
            return Result(false, "Render pipeline prepare failed: render data is null.");

        _render_data = std::move(render_data);
        auto had_recoverable_failures = false;
        auto recoverable_failure_report = std::string {};
        for (const auto& operation : _operations)
        {
            if (!operation)
                continue;

            if (const auto result = operation->prepare(*_render_data); !result)
            {
                had_recoverable_failures = true;
                const auto failure = make_failure_result("prepare", operation->get_debug_info(), result);
                if (!recoverable_failure_report.empty())
                    recoverable_failure_report += "\n";
                recoverable_failure_report += failure.get_report();
            }
        }

        if (had_recoverable_failures)
        {
            return Result(
                true,
                "Render pipeline prepare completed with recoverable operation failures:\n"
                    + recoverable_failure_report);
        }

        return {};
    }

    void RenderPipeline::release()
    {
        auto backend = _backend.lock();
        if (!backend && !_operations.empty())
        {
            TBX_TRACE_ERROR(
                "Render pipeline release: graphics backend is unavailable; skipping operation "
                "release — GPU resources owned by operations may leak until the backend is torn "
                "down.");
        }

        for (auto& operation : _operations)
        {
            if (operation && backend)
                operation->release(*backend);
        }

        _render_data.reset();
        _operations.clear();
    }
}
