#pragma once
#include "tbx/systems/graphics/pipeline/commands/render_command_executor.h"
#include "tbx/systems/graphics/pipeline/context/render_data.h"
#include "tbx/systems/graphics/pipeline/render_operation.h"
#include <string>
#include <vector>

namespace tbx
{
    inline RenderOperationDebugInfo make_pass_debug_info(const std::string& name)
    {
        auto debug_info = RenderOperationDebugInfo();
        debug_info.debug_name = name;
        debug_info.category = "Pass Execution";
        return debug_info;
    }

    inline Result ensure_frame_started(IGraphicsBackend& backend, RenderData& render_data)
    {
        if (!render_data.frame_started)
        {
            return Result(
                false,
                "Render pass execution failed: frame was not started by the renderer.");
        }

        if (!render_data.view_started)
        {
            return Result(
                false,
                "Render pass execution failed: view was not started by the renderer.");
        }

        return {};
    }

    inline Result execute_draw_list(
        IGraphicsBackend& backend,
        RenderData& render_data,
        const std::vector<GraphicsIndexedDrawCommand>& draw_commands,
        const GraphicsPassDesc& pass_desc,
        const CancellationToken& token,
        const std::string& cancel_report)
    {
        if (draw_commands.empty())
            return {};

        if (token && token.is_cancelled())
            return Result(false, cancel_report + " cancelled.");

        if (const auto result = ensure_frame_started(backend, render_data); !result)
            return result;

        if (const auto result = backend.begin_pass(pass_desc); !result)
            return result;

        const auto executor = RenderCommandExecutor();
        for (const auto& draw : draw_commands)
        {
            if (token && token.is_cancelled())
            {
                (void)backend.end_pass();
                return Result(false, cancel_report + " cancelled mid-pass.");
            }

            if (const auto result = executor.execute_indexed_draw(backend, draw); !result)
            {
                (void)backend.end_pass();
                return result;
            }
        }

        return backend.end_pass();
    }

    inline Result execute_render_pass_list(
        IGraphicsBackend& backend,
        RenderData& render_data,
        const std::vector<GraphicsRenderPass>& passes,
        const CancellationToken& token,
        const std::string& cancel_report)
    {
        if (passes.empty())
            return {};

        if (const auto result = ensure_frame_started(backend, render_data); !result)
            return result;

        const auto executor = RenderCommandExecutor();
        for (const auto& pass : passes)
        {
            if (token && token.is_cancelled())
                return Result(false, cancel_report + " cancelled.");

            if (pass.viewport.has_value())
            {
                if (const auto result = backend.set_viewport(pass.viewport.value()); !result)
                    return result;
            }

            if (const auto result = backend.begin_pass(pass.pass); !result)
                return result;

            for (const auto& draw : pass.draws)
            {
                if (token && token.is_cancelled())
                {
                    (void)backend.end_pass();
                    return Result(false, cancel_report + " cancelled mid-pass.");
                }

                if (const auto result = executor.execute_draw(backend, draw); !result)
                {
                    (void)backend.end_pass();
                    return result;
                }
            }

            for (const auto& draw : pass.indexed_draws)
            {
                if (token && token.is_cancelled())
                {
                    (void)backend.end_pass();
                    return Result(false, cancel_report + " cancelled mid-pass.");
                }

                if (const auto result = executor.execute_indexed_draw(backend, draw); !result)
                {
                    (void)backend.end_pass();
                    return result;
                }
            }

            if (const auto result = backend.end_pass(); !result)
                return result;
        }

        return backend.set_viewport(render_data.viewport);
    }

    inline GraphicsPassDesc make_scene_pass_desc(
        const RenderData& render_data,
        GraphicsPassDesc pass_desc)
    {
        if (render_data.gbuffer.has_all_color_targets())
            pass_desc.color_targets = render_data.gbuffer.to_color_target_list();
        else if (render_data.gbuffer.get_color_target().is_valid())
            pass_desc.color_targets = {render_data.gbuffer.get_color_target()};
        if (render_data.gbuffer.depth_target.is_valid())
            pass_desc.depth_stencil_target = render_data.gbuffer.depth_target;
        return pass_desc;
    }

}
