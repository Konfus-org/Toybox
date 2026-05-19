#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/async/cancellation_token.h"
#include "tbx/systems/graphics/pipeline/render_pass.h"
#include "tbx/tbx_api.h"
#include "tbx/utils/result.h"

namespace tbx
{
    /// @brief
    /// Purpose: Submits backend-neutral render pass command lists to the active backend.
    /// @details
    /// Ownership: Stateless helper. Backend resources referenced by commands remain backend-owned.
    /// Thread Safety: Not thread-safe; call from the backend-owning render lane.
    class TBX_API RenderCommandExecutor final
    {
      public:
        Result execute(
            IGraphicsBackend& backend,
            const GraphicsRenderPass& render_pass,
            const CancellationToken& token) const;
    };
}
