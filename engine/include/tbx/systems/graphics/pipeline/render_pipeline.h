#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/pipeline/render_operation.h"
#include "tbx/systems/graphics/pipeline/context/render_data.h"
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
        /// Ownership: Executes against the render data most recently accepted by prepare().
        Result execute(const CancellationToken& token) const;

        /// @brief
        /// Purpose: Runs CPU preparation work for all operations in configured order.
        /// @details
        /// Ownership: Takes ownership of render data for the current pipeline submission.
        Result prepare(std::unique_ptr<RenderData> render_data);

        /// @brief
        /// Purpose: Returns the render data currently owned by the pipeline.
        RenderData* get_render_data() const;

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
        std::unique_ptr<RenderData> _render_data = {};
        std::vector<std::unique_ptr<IRenderOperation>> _operations = {};
    };
}
