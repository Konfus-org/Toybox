#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/pipeline/render_frame_context.h"
#include "tbx/systems/graphics/pipeline/render_operation.h"
#include "tbx/tbx_api.h"
#include "tbx/utils/result.h"
#include <functional>
#include <memory>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Runs typed render operations through prepare, execute, and release phases.
    /// @details
    /// Ownership: Owns operations via unique pointers. Borrows the backend used for execute and
    /// release. Thread Safety: Not thread-safe; call from the owning render thread.
    class TBX_API RenderPipeline final
    {
      public:
        RenderPipeline(IGraphicsBackend& backend);
        ~RenderPipeline() noexcept = default;

      public:
        RenderPipeline(const RenderPipeline&) = delete;
        RenderPipeline& operator=(const RenderPipeline&) = delete;
        RenderPipeline(RenderPipeline&&) noexcept = default;
        RenderPipeline& operator=(RenderPipeline&&) noexcept = default;

      public:
        /// @brief
        /// Purpose: Adds a render operation to the end of the pipeline.
        /// @details
        /// Ownership: Takes unique ownership of the operation.
        void add_operation(std::unique_ptr<IRenderOperation> operation);

        /// @brief
        /// Purpose: Removes all operations without calling release().
        /// @details
        /// Ownership: Destroys stored operations immediately; callers should call release() first
        /// when operations own backend resources.
        void clear();

        /// @brief
        /// Purpose: Executes GPU work for all operations in configured order.
        /// @details
        /// Ownership: Keeps ownership of operations and context. The context parameter is required
        /// by the typed render pipeline contract and validated before executing.
        Result execute(std::shared_ptr<RenderFrameContext> context, const CancellationToken& token)
            const;

        /// @brief
        /// Purpose: Runs CPU preparation work for all operations in configured order.
        /// @details
        /// Ownership: Keeps ownership of operations and context. Operations may write shared
        /// outputs into the context for later operations.
        Result prepare(std::shared_ptr<RenderFrameContext> context);

        /// @brief
        /// Purpose: Releases backend resources owned by operations and clears the pipeline.
        /// @details
        /// Ownership: Keeps the borrowed backend alive externally; clears unique ownership of
        /// operations after release.
        void release();

      private:
        Result make_failure_result(
            const char* phase,
            const RenderOperationDebugInfo& debug_info,
            const Result& result) const;

      private:
        std::reference_wrapper<IGraphicsBackend> _backend;
        std::vector<std::unique_ptr<IRenderOperation>> _operations = {};
    };
}
