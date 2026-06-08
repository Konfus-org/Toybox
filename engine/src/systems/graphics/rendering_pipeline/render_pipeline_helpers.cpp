#include "render_pipeline_helpers.h"

#include "tbx/systems/debugging/macros.h"
#include <utility>

namespace tbx
{
    Result make_render_pipeline_failure(std::string message)
    {
        return Result(false, std::move(message));
    }

    Result warn_render_pipeline_backend_failure(std::string_view context, Result result)
    {
        if (result)
            return result;

        TBX_TRACE_WARNING(
            "Rendering pipeline: {} ({})",
            context,
            result.get_report().empty() ? "backend returned failure" : result.get_report());
        return result;
    }
}
