#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/tbx_api.h"
#include "tbx/types/uuid.h"
#include "tbx/types/viewport.h"
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
        Uuid index_buffer = {};
        GraphicsIndexType index_type = GraphicsIndexType::UINT32;
        std::vector<GraphicsResourceBinding> vertex_buffers = {};
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
}
