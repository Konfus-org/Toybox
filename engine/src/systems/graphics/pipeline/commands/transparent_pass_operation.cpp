#include "tbx/systems/graphics/pipeline/commands/transparent_pass_operation.h"
#include "pass_operation_helpers.h"

namespace tbx
{
    RenderOperationDebugInfo TransparentPassOperation::get_debug_info() const
    {
        return make_pass_debug_info("Toybox Transparent Pass Operation");
    }

    Result TransparentPassOperation::prepare(RenderData& render_data)
    {
        render_data.transparent_commands.clear();
        return {};
    }

    Result TransparentPassOperation::execute(
        IGraphicsBackend& backend,
        RenderData& render_data,
        const CancellationToken& token)
    {
        auto pass_desc = GraphicsPassDesc {
            .clear_color = Color::BLACK,
            .clear_depth = 1.0F,
            .clear_stencil = 0U,
            .clear_flags = GraphicsClearFlags::NONE,
            .debug_name = "Toybox Transparent Pass",
        };
        if (render_data.gbuffer.get_color_target().is_valid())
            pass_desc.color_targets = {render_data.gbuffer.get_color_target()};
        if (render_data.gbuffer.depth_target.is_valid())
            pass_desc.depth_stencil_target = render_data.gbuffer.depth_target;

        return execute_draw_list(
            backend,
            render_data,
            render_data.transparent_commands,
            pass_desc,
            token,
            "TransparentPassOperation");
    }
}
