#include "tbx/core/systems/pipeline.h"
#include "tbx/core/types/typedefs.h"
#include <iostream>
#include <string>

namespace tbx
{
    static Result make_cancellation_result()
    {
        return Result(false, "Pipeline execution cancelled.");
    }

    static void log_pipeline_failure(size operation_index, const Result& result)
    {
        auto report = result.get_report();
        if (report.empty())
            report = "Operation returned failure without a report.";

        std::clog << "Pipeline operation " << operation_index << " failed: " << report << '\n';
    }

    void Pipeline::add_operation(std::unique_ptr<PipelineOperation> operation)
    {
        if (!operation)
            return;

        _operations.push_back(std::move(operation));
    }

    void Pipeline::clear_operations()
    {
        _operations.clear();
    }

    const std::vector<std::unique_ptr<PipelineOperation>>& Pipeline::get_operations() const
    {
        return _operations;
    }

    Result Pipeline::execute(
        const std::any& payload,
        const CancellationToken& cancellation_token)
    {
        const auto& pipeline = *this;
        return pipeline.execute(payload, cancellation_token);
    }

    Result Pipeline::execute(
        const std::any& payload,
        const CancellationToken& cancellation_token) const
    {
        size operation_index = 0U;
        for (const auto& operation : _operations)
        {
            if (cancellation_token && cancellation_token.is_cancelled())
                return make_cancellation_result();

            if (!operation)
            {
                ++operation_index;
                continue;
            }

            auto result = operation->execute(payload, cancellation_token);
            if (!result)
            {
                log_pipeline_failure(operation_index, result);
                return result;
            }

            ++operation_index;
        }

        return {};
    }

    Result Pipeline::execute(const std::any& payload) const
    {
        return execute(payload, CancellationToken {});
    }

    Result Pipeline::execute() const
    {
        return execute(std::any {});
    }
}
