#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/render_pass.h"
#include "tbx/tbx_api.h"
#include "tbx/utils/result.h"
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Consumes backend-neutral render passes and submits their draw commands.
    /// @details
    /// Ownership: Stateless executor; draw commands own all submission data by value.
    /// Thread Safety: Not inherently thread-safe; execute on the render lane.
    class TBX_API DrawCommandExecutor final
    {
      public:
        DrawCommandExecutor() = default;
        ~DrawCommandExecutor() = default;

      public:
        /// @brief
        /// Purpose: Executes every pass and draw command in order.
        Result execute(IGraphicsBackend& backend, const std::vector<RenderPass>& render_passes)
            const;

      private:
        Result execute_draw_command(
            IGraphicsBackend& backend,
            const GraphicsDrawCommand& command) const;
        Result execute_draw_command(
            IGraphicsBackend& backend,
            const GraphicsIndexedDrawCommand& command) const;
    };
}
