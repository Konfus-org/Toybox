#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/pipeline/render_operation.h"
#include "tbx/tbx_api.h"
#include "tbx/utils/result.h"
#include <memory>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Runs typed render operations through prepare and execute phases.
    /// @details
    /// Ownership: Owns operations via unique pointers. Borrows the backend used for execute.
    /// Thread Safety: Not thread-safe; call from the owning render thread.
    class TBX_API RenderPipeline final
    {
      public:
        explicit RenderPipeline(std::weak_ptr<IGraphicsBackend> backend);
        ~RenderPipeline() noexcept = default;

      public:
        RenderPipeline(const RenderPipeline&) = delete;
        RenderPipeline& operator=(const RenderPipeline&) = delete;
        RenderPipeline(RenderPipeline&&) noexcept = default;
        RenderPipeline& operator=(RenderPipeline&&) noexcept = default;

      public:
        void add_operation(std::unique_ptr<IRenderOperation> operation);
        void clear();

        // TODO: Result run(FrameData& frame_data, const CancellationToken& token);

      private:
        /// @brief
        /// Purpose: Executes GPU work for all operations in configured order.
        /// @details
        /// Ownership: Executes against the render data most recently accepted by prepare().
        Result execute(FrameData& frame_data, const CancellationToken& token) const;

        /// @brief
        /// Purpose: Runs CPU preparation work for all operations in configured order.
        /// @details
        /// Ownership: Takes ownership of render data for the current pipeline submission.
        Result prepare(FrameData& frame_data, const CancellationToken& token);

        Result make_failure_result(
            const char* phase,
            const RenderOperationDebugInfo& debug_info,
            const Result& result) const;

      private:
        std::weak_ptr<IGraphicsBackend> _backend;
        std::vector<std::unique_ptr<IRenderOperation>> _operations = {};
    };
}
