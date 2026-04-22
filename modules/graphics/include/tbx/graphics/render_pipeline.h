#pragma once
#include "tbx/common/pipeline.h"
#include "tbx/common/result.h"
#include "tbx/common/uuid.h"
#include "tbx/graphics/graphics_backend.h"
#include "tbx/graphics/viewport.h"
#include "tbx/tbx_api.h"
#include <any>
#include <memory>
#include <optional>
#include <vector>

namespace tbx
{
    struct TBX_API GraphicsDrawCommand
    {
        Uuid pipeline = {};
        std::vector<GraphicsResourceBinding> vertex_buffers = {};
        std::vector<GraphicsResourceBinding> uniform_buffers = {};
        std::vector<GraphicsResourceBinding> storage_buffers = {};
        std::vector<GraphicsResourceBinding> textures = {};
        std::vector<GraphicsResourceBinding> samplers = {};
        uint32 vertex_count = 0U;
        uint32 vertex_offset = 0U;
    };

    struct TBX_API GraphicsIndexedDrawCommand
    {
        Uuid pipeline = {};
        std::vector<GraphicsResourceBinding> vertex_buffers = {};
        Uuid index_buffer = {};
        GraphicsIndexType index_type = GraphicsIndexType::UINT32;
        std::vector<GraphicsResourceBinding> uniform_buffers = {};
        std::vector<GraphicsResourceBinding> storage_buffers = {};
        std::vector<GraphicsResourceBinding> textures = {};
        std::vector<GraphicsResourceBinding> samplers = {};
        GraphicsDrawIndexedDesc draw = {};
    };

    struct TBX_API GraphicsRenderPass
    {
        GraphicsPassDesc pass = {};
        std::optional<Viewport> viewport = std::nullopt;
        std::vector<GraphicsDrawCommand> draws = {};
        std::vector<GraphicsIndexedDrawCommand> indexed_draws = {};
    };

    struct TBX_API GraphicsPipelineExecutionContext
    {
        IGraphicsBackend* backend = nullptr;
        Result* result = nullptr;
    };

    class TBX_API GraphicsRenderPassOperation final : public PipelineOperation
    {
      public:
        GraphicsRenderPassOperation(GraphicsRenderPass pass);

      public:
        void execute(const std::any& payload) override;
        const GraphicsRenderPass& get_pass() const;

      private:
        GraphicsRenderPass _pass = {};
    };

    class TBX_API GraphicsRenderPipeline
    {
      public:
        void add_operation(std::unique_ptr<PipelineOperation> operation);
        void add_pass_operation(GraphicsRenderPass pass);
        void clear();

        Result execute(IGraphicsBackend& backend) const;

        const Pipeline& get_pipeline() const;

      private:
        mutable Pipeline _pipeline = {};
    };
}
