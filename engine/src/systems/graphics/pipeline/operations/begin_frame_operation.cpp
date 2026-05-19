#include "tbx/systems/graphics/pipeline/operations/begin_frame_operation.h"

namespace tbx
{
RenderOperationDebugInfo BeginFrameOperation::get_debug_info() const { return {.debug_name="Begin Frame", .category="Frame"}; }
Result BeginFrameOperation::prepare(FrameData&) { return {}; }
Result BeginFrameOperation::execute(IGraphicsBackend& backend, FrameData& frame_data, const CancellationToken& token)
{
    // Begin frame/view before any render pass work.
    if (token.is_cancelled()) return Result(false, "Begin frame cancelled.");
    auto result = backend.begin_frame(GraphicsFrameInfo{.output_window=frame_data.output_window, .render_resolution=frame_data.render_resolution, .output_resolution=frame_data.output_resolution});
    if (!result) return result;
    result = backend.begin_view(frame_data.view);
    if (!result) return result;
    return backend.set_viewport(frame_data.view.viewport);
}
}
