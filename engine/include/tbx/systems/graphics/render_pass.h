#pragma once
#include "tbx/interfaces/graphics_backend.h"

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
        std::vector<Uuid> bind_groups = {};
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
        std::vector<Uuid> bind_groups = {};
        GraphicsDrawIndexedDesc draw = {};
    };

    struct TBX_API GraphicsComputeCommand
    {
        Uuid pipeline = {};
        std::vector<Uuid> bind_groups = {};
        uint32 group_count_x = 1U;
        uint32 group_count_y = 1U;
        uint32 group_count_z = 1U;
        std::string debug_name = {};
    };

    struct TBX_API RenderPass
    {
        GraphicsPassDesc desc = {};
        std::vector<GraphicsDrawCommand> draws = {};
        std::vector<GraphicsIndexedDrawCommand> indexed_draws = {};
        std::vector<PipelineBarrierDesc> barriers_before = {};
        std::vector<PipelineBarrierDesc> barriers_after = {};
    };

    struct TBX_API ComputePass
    {
        std::string debug_name = {};
        std::vector<PipelineBarrierDesc> barriers_before = {};
        std::vector<GraphicsComputeCommand> dispatches = {};
        std::vector<PipelineBarrierDesc> barriers_after = {};
    };
}
