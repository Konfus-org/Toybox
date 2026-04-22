#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/viewport.h"
#include "tbx/systems/pipeline.h"
#include "tbx/tbx_api.h"
#include "tbx/types/uuid.h"
#include "tbx/utils/result.h"
#include <any>
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

    struct TBX_API GraphicsPipelinePayload
    {
        IGraphicsBackend* backend = nullptr;
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
        IGraphicsBackend* _backend = nullptr;
    };
}
