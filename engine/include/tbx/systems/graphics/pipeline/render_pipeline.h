#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/pipeline/context/frame_data.h"
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

        Result run(FrameData& frame_data, const CancellationToken& token);

        /// @brief
        /// Purpose: Runs CPU preparation work for all operations in configured order.
        Result prepare(FrameData& frame_data, const CancellationToken& token);
        Result prepare(std::unique_ptr<FrameData> frame_data);

        /// @brief
        /// Purpose: Executes GPU work for all operations in configured order.
        Result execute(FrameData& frame_data, const CancellationToken& token) const;
        Result execute(const CancellationToken& token) const;

      private:
        Result make_failure_result(
            const char* phase,
            const RenderOperationDebugInfo& debug_info,
            const Result& result) const;

      private:
        std::weak_ptr<IGraphicsBackend> _backend;
        std::vector<std::unique_ptr<IRenderOperation>> _operations = {};
        std::unique_ptr<FrameData> _prepared_frame_data = nullptr;
    };
}
