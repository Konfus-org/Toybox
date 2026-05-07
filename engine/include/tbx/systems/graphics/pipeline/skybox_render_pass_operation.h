#pragma once
#include "tbx/systems/graphics/pipeline/render_pass_operation.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    class TBX_API SkyboxRenderPassOperation final : public PipelineOperation
    {
      public:
        SkyboxRenderPassOperation(
            Uuid pipeline,
            Uuid vertex_buffer,
            Uuid index_buffer,
            Uuid instance_buffer,
            Uuid view_uniform_buffer,
            Uuid material_uniform_buffer,
            uint32 index_count,
            std::vector<GraphicsResourceBinding> textures);

      public:
        Result execute(const std::any& payload, const CancellationToken& cancellation_token)
            override;
        const GraphicsRenderPass& get_pass() const;

      private:
        GraphicsRenderPassOperation _operation;
    };
}
