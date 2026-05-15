#include "tbx/systems/graphics/pipeline/commands/alpha_cutout_pass_operation.h"
#include "pass_operation_helpers.h"

namespace tbx
{
    RenderOperationDebugInfo AlphaCutoutPassOperation::get_debug_info() const
    {
        return make_pass_debug_info("Toybox Alpha Cutout Pass Operation");
    }

    Result AlphaCutoutPassOperation::prepare(RenderData& render_data)
    {
        render_data.alpha_cutout_commands.clear();
        return {};
    }

    Result AlphaCutoutPassOperation::execute(
        IGraphicsBackend& backend,
        RenderData& render_data,
        const CancellationToken& token)
    {
        return execute_draw_list(
            backend,
            render_data,
            render_data.alpha_cutout_commands,
            make_scene_pass_desc(
                render_data,
                GraphicsPassDesc {
                    .clear_color = Color::BLACK,
                    .clear_depth = 1.0F,
                    .clear_stencil = 0U,
                    .clear_flags = GraphicsClearFlags::NONE,
                    .debug_name = "Toybox Alpha Cutout Pass",
                }),
            token,
            "AlphaCutoutPassOperation");
    }
}
