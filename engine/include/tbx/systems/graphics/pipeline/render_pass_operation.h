#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/pipeline/render_pass.h"
#include "tbx/utils/pipeline.h"
#include "tbx/utils/result.h"
#include <any>
#include <functional>

namespace tbx
{
    struct TBX_API GraphicsPipelinePayload
    {
        std::reference_wrapper<IGraphicsBackend> backend;
    };

    class TBX_API GraphicsRenderPassOperation final : public PipelineOperation
    {
      public:
        GraphicsRenderPassOperation(GraphicsRenderPass pass);

      public:
        Result execute(const std::any& payload, const CancellationToken& cancellation_token)
            override;
        const GraphicsRenderPass& get_pass() const;

      private:
        GraphicsRenderPass _pass = {};
    };
}
