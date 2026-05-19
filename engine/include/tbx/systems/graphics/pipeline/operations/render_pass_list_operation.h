#pragma once
#include "tbx/systems/graphics/pipeline/render_operation.h"
#include <string>

namespace tbx
{
    class TBX_API RenderPassListOperation final : public IRenderOperation
    {
      public:
        using PassListSelector = std::vector<GraphicsRenderPass> FrameData::*;
        RenderPassListOperation(std::string debug_name, std::string category, PassListSelector pass_list_selector);
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(FrameData& frame_data) override;
        Result execute(IGraphicsBackend& backend, FrameData& frame_data, const CancellationToken& token) override;

      private:
        RenderOperationDebugInfo _debug_info = {};
        PassListSelector _pass_list_selector = nullptr;
    };
}
