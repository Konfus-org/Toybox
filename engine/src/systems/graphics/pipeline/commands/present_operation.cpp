#include "tbx/systems/graphics/pipeline/commands/present_operation.h"
#include "pass_operation_helpers.h"

namespace tbx
{
    RenderOperationDebugInfo PresentOperation::get_debug_info() const
    {
        return make_pass_debug_info("Toybox Present Operation");
    }

    Result PresentOperation::prepare(RenderData&)
    {
        return {};
    }

    Result PresentOperation::execute(
        IGraphicsBackend& backend,
        RenderData& render_data,
        const CancellationToken& token)
    {
        auto& frame_data = render_data;
        if (token && token.is_cancelled())
            return Result(false, "PresentOperation cancelled.");

        if (const auto result = ensure_frame_started(backend, render_data); !result)
            return result;

        if (frame_data.view_started)
        {
            if (const auto result = backend.end_view(); !result)
                return result;
            frame_data.view_started = false;
        }

        if (const auto result = backend.present(); !result)
            return result;

        if (frame_data.frame_started)
        {
            if (const auto result = backend.end_frame(); !result)
                return result;
            frame_data.frame_started = false;
        }

        return {};
    }
}
