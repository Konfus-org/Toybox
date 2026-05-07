#include "tbx/systems/graphics/pipeline/geometry_render_pass_operation.h"
#include <utility>

namespace tbx
{
    static GraphicsRenderPass make_geometry_render_pass(
        const bool clear_color,
        const uint32 index_count,
        const Uuid& geometry_pipeline,
        const Uuid& geometry_vertex_buffer,
        const Uuid& geometry_index_buffer,
        std::vector<GraphicsIndexedDrawCommand> indexed_draws)
    {
        auto geometry_pass = GraphicsRenderPass {
            .pass =
                GraphicsPassDesc {
                    .clear_color = Color::BLACK,
                    .clear_depth = 1.0F,
                    .clear_stencil = 0U,
                    .clear_flags =
                        clear_color ? GraphicsClearFlags::COLOR_DEPTH
                                    : GraphicsClearFlags::DEPTH,
                    .debug_name = "Toybox Geometry Pass",
                },
        };

        if (index_count > 0U && geometry_pipeline.is_valid() && geometry_vertex_buffer.is_valid()
            && geometry_index_buffer.is_valid())
        {
            geometry_pass.indexed_draws.push_back(
                GraphicsIndexedDrawCommand {
                    .pipeline = geometry_pipeline,
                    .vertex_buffers =
                        {
                            GraphicsResourceBinding {
                                .slot = 0U,
                                .resource = geometry_vertex_buffer,
                            },
                        },
                    .index_buffer = geometry_index_buffer,
                    .index_type = GraphicsIndexType::UINT32,
                    .draw =
                        GraphicsDrawIndexedDesc {
                            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                            .index_type = GraphicsIndexType::UINT32,
                            .index_count = index_count,
                            .instance_count = 1U,
                        },
                });
        }

        for (auto& draw : indexed_draws)
            geometry_pass.indexed_draws.push_back(std::move(draw));

        return geometry_pass;
    }

    GeometryRenderPassOperation::GeometryRenderPassOperation(
        const bool clear_color,
        const uint32 index_count,
        Uuid geometry_pipeline,
        Uuid geometry_vertex_buffer,
        Uuid geometry_index_buffer,
        std::vector<GraphicsIndexedDrawCommand> indexed_draws)
        : _operation(make_geometry_render_pass(
              clear_color,
              index_count,
              geometry_pipeline,
              geometry_vertex_buffer,
              geometry_index_buffer,
              std::move(indexed_draws)))
    {
    }

    Result GeometryRenderPassOperation::execute(
        const std::any& payload,
        const CancellationToken& cancellation_token)
    {
        return _operation.execute(payload, cancellation_token);
    }

    const GraphicsRenderPass& GeometryRenderPassOperation::get_pass() const
    {
        return _operation.get_pass();
    }
}
