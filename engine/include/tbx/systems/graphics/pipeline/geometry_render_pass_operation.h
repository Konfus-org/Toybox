#pragma once
#include "tbx/systems/graphics/pipeline/render_pass_operation.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    class TBX_API GeometryRenderPassOperation final : public PipelineOperation
    {
      public:
        GeometryRenderPassOperation(
            bool clear_color,
            uint32 index_count,
            Uuid geometry_pipeline,
            Uuid geometry_vertex_buffer,
            Uuid geometry_index_buffer,
            std::vector<GraphicsIndexedDrawCommand> indexed_draws);

      public:
        Result execute(const std::any& payload, const CancellationToken& cancellation_token)
            override;
        const GraphicsRenderPass& get_pass() const;

      private:
        GraphicsRenderPassOperation _operation;
    };
}
