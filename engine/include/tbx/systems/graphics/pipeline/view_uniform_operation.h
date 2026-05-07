#pragma once
#include "tbx/systems/graphics/pipeline/render_operation.h"
#include "tbx/tbx_api.h"
#include "tbx/types/uuid.h"

namespace tbx
{
    /// @brief
    /// Purpose: Allocates and updates the per-frame view/projection uniform buffer.
    /// @details
    /// Ownership: Owns _buffer for the full lifetime of the operation.
    /// Interaction: Writes context.view_uniform_buffer during prepare() so subsequent
    /// operations can bind it without coupling to this type.
    class TBX_API ViewUniformOperation final : public IRenderOperation
    {
      public:
        ViewUniformOperation() = default;
        ~ViewUniformOperation() noexcept override = default;
        ViewUniformOperation(ViewUniformOperation&&) noexcept = default;
        ViewUniformOperation& operator=(ViewUniformOperation&&) noexcept = default;

        Result prepare(RenderFrameContext& context) override;
        Result execute(IGraphicsBackend& backend, const CancellationToken& token) override;
        void release(IGraphicsBackend& backend) override;

      private:
        Uuid _buffer = {};
    };
}
