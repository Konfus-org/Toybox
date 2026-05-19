#pragma once
#include "tbx/systems/graphics/pipeline/render_operation.h"

namespace tbx
{
    class TBX_API BeginFrameOperation final : public IRenderOperation
    {
      public:
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(FrameData& frame_data) override;
        Result execute(
            IGraphicsBackend& backend,
            FrameData& frame_data,
            const CancellationToken& token) override;
    };
}
