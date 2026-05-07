#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/pipeline/render_pass_operation.h"
#include "tbx/tbx_api.h"
#include "tbx/utils/pipeline.h"
#include "tbx/utils/result.h"
#include <any>

namespace tbx
{
    class TBX_API GraphicsRenderPipeline final : public Pipeline
    {
      public:
        GraphicsRenderPipeline(IGraphicsBackend& backend);

      public:
        GraphicsRenderPipeline(const GraphicsRenderPipeline&) = delete;
        GraphicsRenderPipeline& operator=(const GraphicsRenderPipeline&) = delete;
        GraphicsRenderPipeline(GraphicsRenderPipeline&&) noexcept = delete;
        GraphicsRenderPipeline& operator=(GraphicsRenderPipeline&&) noexcept = delete;

      public:
        void add_pass_operation(GraphicsRenderPass pass);
        void clear();

        Result execute() const;
        Result execute(const CancellationToken& cancellation_token) const;
        IGraphicsBackend& get_backend() const;

      private:
        Result execute(const std::any& payload, const CancellationToken& cancellation_token)
            override;

      private:
        IGraphicsBackend& _backend;
    };
}
