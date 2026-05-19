#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/pipeline/context/frame_data.h"
#include "tbx/systems/async/cancellation_token.h"
#include "tbx/tbx_api.h"
#include "tbx/utils/result.h"
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Describes render operation identity for diagnostics and pipeline composition.
    struct TBX_API RenderOperationDebugInfo
    {
        std::string debug_name = "Unnamed Render Operation";
        std::string category = "Render";
    };

    /// @brief
    /// Purpose: Defines a self-contained unit of rendering work with explicit prepare and execute
    /// phases.
    /// @details
    /// Ownership: Owns CPU-side operation state. GPU resources are owned by
    /// GraphicsResourceManager. Thread Safety: Not thread-safe; call from the owning render thread.
    class TBX_API IRenderOperation
    {
      public:
        IRenderOperation() = default;
        virtual ~IRenderOperation() noexcept = default;
        IRenderOperation(const IRenderOperation&) = delete;
        IRenderOperation& operator=(const IRenderOperation&) = delete;
        IRenderOperation(IRenderOperation&&) noexcept = delete;
        IRenderOperation& operator=(IRenderOperation&&) noexcept = delete;

        /// @brief
        /// Purpose: Returns debug info for logs, tooling, and debug labels.
        virtual RenderOperationDebugInfo get_debug_info() const = 0;

        /// @brief
        /// Purpose: CPU phase — queries scene state, uploads or updates GPU resources.
        /// @details
        /// Reads and writes RenderData fields. Must complete for all operations before execute.
        virtual Result prepare(FrameData& render_data) = 0;

        /// @brief
        /// Purpose: GPU phase — opens a render pass, binds resources, issues draw calls, closes the
        /// pass.
        /// @details
        /// Consumes draw lists and pass inputs written into RenderData by earlier operations.
        virtual Result execute(
            IGraphicsBackend& backend,
            FrameData& render_data,
            const CancellationToken& token) = 0;
    };
}
