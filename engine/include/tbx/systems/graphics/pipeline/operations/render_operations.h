#pragma once
#include "tbx/systems/graphics/pipeline/operations/render_command_executor.h"
#include "tbx/systems/graphics/pipeline/render_operation.h"
#include <string>

namespace tbx
{
    /// @brief Purpose: Opens backend frame and view state for a prepared FrameData.
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

    /// @brief Purpose: Submits a named group of render passes from FrameData.
    class TBX_API RenderPassListOperation final : public IRenderOperation
    {
      public:
        using PassListSelector = std::vector<GraphicsRenderPass> FrameData::*;

        RenderPassListOperation(
            std::string debug_name,
            std::string category,
            PassListSelector pass_list_selector);

        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(FrameData& frame_data) override;
        Result execute(
            IGraphicsBackend& backend,
            FrameData& frame_data,
            const CancellationToken& token) override;

      private:
        RenderOperationDebugInfo _debug_info = {};
        PassListSelector _pass_list_selector = nullptr;
        RenderCommandExecutor _executor = {};
    };

    /// @brief Purpose: Closes view/frame state and presents the prepared frame.
    class TBX_API EndFrameOperation final : public IRenderOperation
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
