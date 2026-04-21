#pragma once
#include "tbx/common/result.h"
#include "tbx/common/uuid.h"
#include "tbx/graphics/graphics_backend.h"
#include "tbx/graphics/viewport.h"
#include "tbx/tbx_api.h"
#include <optional>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Describes one indexed draw submitted by Toybox render passes.
    /// @details
    /// Ownership: Stores backend resource ids and binding slots by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
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

    /// @brief
    /// Purpose: Describes one non-indexed draw submitted by Toybox render passes.
    /// @details
    /// Ownership: Stores backend resource ids and binding slots by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
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

    /// @brief
    /// Purpose: Describes one backend-neutral render pass owned by Toybox.
    /// @details
    /// Ownership: Owns pass state and draw command lists by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API GraphicsRenderPass
    {
        GraphicsPassDesc pass = {};
        std::optional<Viewport> viewport = std::nullopt;
        std::vector<GraphicsDrawCommand> draws = {};
        std::vector<GraphicsIndexedDrawCommand> indexed_draws = {};
    };

    /// @brief
    /// Purpose: Executes Toybox-owned render passes through an explicit graphics backend.
    /// @details
    /// Ownership: Owns pass descriptions and command lists by value; does not own backend
    /// resources.
    /// Thread Safety: Not inherently thread-safe; callers should synchronize mutation/execution.
    class TBX_API GraphicsRenderPipeline
    {
      public:
        void add_pass(GraphicsRenderPass pass);
        void clear();
        const std::vector<GraphicsRenderPass>& get_passes() const;
        Result execute(IGraphicsBackend& backend) const;

      private:
        std::vector<GraphicsRenderPass> _passes = {};
    };
}
