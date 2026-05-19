#include "tbx/systems/graphics/pipeline/operations/end_frame_operation.h"

namespace tbx
{
RenderOperationDebugInfo EndFrameOperation::get_debug_info() const { return {.debug_name="End Frame", .category="Frame"}; }
Result EndFrameOperation::prepare(FrameData&) { return {}; }
Result EndFrameOperation::execute(IGraphicsBackend& backend, FrameData&, const CancellationToken& token)
{
    // End frame/view and present after all passes are submitted.
    if (token.is_cancelled()) return Result(false, "End frame cancelled.");
    auto result = backend.end_view(); if (!result) return result;
    result = backend.present(); if (!result) return result;
    return backend.end_frame();
}
}
