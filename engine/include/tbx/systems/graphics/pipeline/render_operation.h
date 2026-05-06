#pragma once
#include "tbx/systems/async/cancellation_token.h"
#include "tbx/tbx_api.h"
#include "tbx/utils/result.h"
#include <string>

namespace tbx
{
    class IGraphicsBackend;
    struct RenderFrameContext;

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
    /// Ownership: Owns all GPU resource handles it requires. Releases them via release().
    /// Thread Safety: Not thread-safe; call from the owning render thread.
    class TBX_API IRenderOperation
    {
      public:
        virtual ~IRenderOperation() noexcept = default;
        IRenderOperation(const IRenderOperation&) = delete;
        IRenderOperation& operator=(const IRenderOperation&) = delete;
        IRenderOperation(IRenderOperation&&) noexcept = default;
        IRenderOperation& operator=(IRenderOperation&&) noexcept = default;

        /// @brief
        /// Purpose: Returns debug info for logs, tooling, and debug labels.
        virtual RenderOperationDebugInfo get_debug_info() const = 0;

        /// @brief
        /// Purpose: CPU phase — queries scene state, uploads or updates GPU resources.
        /// @details
        /// Reads from context (camera, frame index) and writes outputs back (e.g.
        /// view_uniform_buffer). Must be called after begin_frame and before execute.
        virtual Result prepare(RenderFrameContext& context) = 0;

        /// @brief
        /// Purpose: GPU phase — opens a render pass, binds resources, issues draw calls, closes the
        /// pass.
        /// @details
        /// Uses only data stored during prepare(); does not read from the context.
        virtual Result execute(IGraphicsBackend& backend, const CancellationToken& token) = 0;

        /// @brief
        /// Purpose: Unloads all owned GPU resources from the backend.
        /// @details
        /// Called before backend shutdown or when the operation is destroyed.
        virtual void release(IGraphicsBackend& backend) {}

      protected:
        IRenderOperation() = default;
    };
}
