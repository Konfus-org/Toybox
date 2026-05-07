#include "tbx/systems/graphics/pipeline/skybox_render_pass_operation.h"
#include <utility>

namespace tbx
{
    static GraphicsRenderPass make_skybox_render_pass(
        const Uuid& pipeline,
        const Uuid& vertex_buffer,
        const Uuid& index_buffer,
        const Uuid& instance_buffer,
        const Uuid& view_uniform_buffer,
        const Uuid& material_uniform_buffer,
        const uint32 index_count,
        std::vector<GraphicsResourceBinding> textures)
    {
        return GraphicsRenderPass {
            .pass =
                GraphicsPassDesc {
                    .clear_color = Color::BLACK,
                    .clear_depth = 1.0F,
                    .clear_stencil = 0U,
                    .clear_flags = GraphicsClearFlags::COLOR_DEPTH,
                    .debug_name = "Toybox Skybox Pass",
                },
            .indexed_draws =
                {
                    GraphicsIndexedDrawCommand {
                        .pipeline = pipeline,
                        .vertex_buffers =
                            {
                                GraphicsResourceBinding {
                                    .slot = 0U,
                                    .resource = vertex_buffer,
                                },
                                GraphicsResourceBinding {
                                    .slot = 1U,
                                    .resource = instance_buffer,
                                },
                            },
                        .index_buffer = index_buffer,
                        .index_type = GraphicsIndexType::UINT32,
                        .uniform_buffers =
                            {
                                GraphicsResourceBinding {
                                    .slot = 0U,
                                    .resource = view_uniform_buffer,
                                },
                                GraphicsResourceBinding {
                                    .slot = 1U,
                                    .resource = material_uniform_buffer,
                                },
                            },
                        .textures = std::move(textures),
                        .draw =
                            GraphicsDrawIndexedDesc {
                                .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                                .index_type = GraphicsIndexType::UINT32,
                                .index_count = index_count,
                                .instance_count = 1U,
                            },
                    },
                },
        };
    }

    SkyboxRenderPassOperation::SkyboxRenderPassOperation(
        Uuid pipeline,
        Uuid vertex_buffer,
        Uuid index_buffer,
        Uuid instance_buffer,
        Uuid view_uniform_buffer,
        Uuid material_uniform_buffer,
        const uint32 index_count,
        std::vector<GraphicsResourceBinding> textures)
        : _operation(make_skybox_render_pass(
              pipeline,
              vertex_buffer,
              index_buffer,
              instance_buffer,
              view_uniform_buffer,
              material_uniform_buffer,
              index_count,
              std::move(textures)))
    {
    }

    Result SkyboxRenderPassOperation::execute(
        const std::any& payload,
        const CancellationToken& cancellation_token)
    {
        return _operation.execute(payload, cancellation_token);
    }

    const GraphicsRenderPass& SkyboxRenderPassOperation::get_pass() const
    {
        return _operation.get_pass();
    }
}
