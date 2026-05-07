#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/pipeline/commands/render_pass.h"
#include "tbx/tbx_api.h"
#include "tbx/utils/result.h"

namespace tbx
{
    /// @brief
    /// Purpose: Executes prepared draw commands by applying their shared binding layout before
    /// issuing a draw.
    /// @details
    /// Ownership: Does not own command resources. The backend must keep referenced resources alive.
    /// Thread Safety: Not thread-safe; call from the owning render thread.
    class TBX_API RenderCommandExecutor
    {
      public:
        Result execute_draw(IGraphicsBackend& backend, const GraphicsDrawCommand& command) const;
        Result execute_indexed_draw(
            IGraphicsBackend& backend,
            const GraphicsIndexedDrawCommand& command) const;

      private:
        Result bind_common_resources(
            IGraphicsBackend& backend,
            const std::vector<GraphicsResourceBinding>& uniform_buffers,
            const std::vector<GraphicsResourceBinding>& storage_buffers,
            const std::vector<GraphicsResourceBinding>& textures,
            const std::vector<GraphicsResourceBinding>& samplers) const;

        Result bind_vertex_buffers(
            IGraphicsBackend& backend,
            const std::vector<GraphicsResourceBinding>& vertex_buffers) const;
    };
}
