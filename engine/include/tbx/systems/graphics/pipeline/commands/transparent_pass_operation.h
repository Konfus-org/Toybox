#pragma once
#include "tbx/systems/graphics/pipeline/render_operation.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    class TBX_API TransparentPassOperation final : public IRenderOperation
    {
      public:
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(RenderData& render_data) override;
        Result execute(
            IGraphicsBackend& backend,
            RenderData& render_data,
            const CancellationToken& token) override;
    };

}
