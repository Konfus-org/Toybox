#include "tbx/systems/graphics/pipeline/commands/pass_operations.h"
#include "tbx/systems/graphics/pipeline/commands/render_command_executor.h"
#include "tbx/systems/graphics/pipeline/context/render_data.h"

namespace tbx
{
    static RenderOperationDebugInfo make_pass_debug_info(const std::string& name)
    {
        auto debug_info = RenderOperationDebugInfo();
        debug_info.debug_name = name;
        debug_info.category = "Pass Execution";
        return debug_info;
    }

    static Result ensure_frame_started(IGraphicsBackend& backend, RenderData& render_data)
    {
        auto& frame_data = render_data.frame;
        if (!frame_data.frame_started)
        {
            if (const auto result = backend.begin_frame(
                    GraphicsFrameInfo {
                        .output_window = frame_data.output_window,
                        .render_resolution = frame_data.render_resolution,
                        .output_resolution = frame_data.render_resolution,
                    });
                !result)
                return result;
            frame_data.frame_started = true;
        }

        if (!frame_data.view_started)
        {
            if (const auto result = backend.begin_view(
                    GraphicsView {
                        .camera = frame_data.camera,
                        .viewport = frame_data.viewport,
                    });
                !result)
                return result;

            frame_data.view_started = true;
            if (const auto result = backend.set_viewport(frame_data.viewport); !result)
                return result;
        }

        return {};
    }

    static Result execute_draw_list(
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
                return backend.end_pass(), Result(false, cancel_report + " cancelled mid-pass.");

            if (const auto result = executor.execute_indexed_draw(backend, draw); !result)
                return backend.end_pass(), result;
        }

        return backend.end_pass();
    }

    static Result execute_shadow_pass_list(
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

            for (const auto& draw : pass.indexed_draws)
            {
                if (token && token.is_cancelled())
                {
                    backend.end_pass();
                    return Result(false, cancel_report + " cancelled mid-pass.");
                }

                if (const auto result = executor.execute_indexed_draw(backend, draw); !result)
                    return backend.end_pass(), result;
            }

            if (const auto result = backend.end_pass(); !result)
                return result;
        }

        return {};
    }

    RenderOperationDebugInfo ExecuteSkyboxPassOperation::get_debug_info() const
    {
        return make_pass_debug_info("Toybox Execute Skybox Pass Operation");
    }

    RenderOperationDebugInfo ExecuteDirectionalShadowPassOperation::get_debug_info() const
    {
        return make_pass_debug_info("Toybox Execute Directional Shadow Pass Operation");
    }

    Result ExecuteDirectionalShadowPassOperation::prepare(RenderData&)
    {
        return {};
    }

    Result ExecuteDirectionalShadowPassOperation::execute(
        IGraphicsBackend& backend,
        RenderData& render_data,
        const CancellationToken& token)
    {
        if (const auto result = execute_shadow_pass_list(
                backend,
                render_data,
                render_data.directional_shadow_passes,
                token,
                "ExecuteDirectionalShadowPassOperation (directional)");
            !result)
        {
            return result;
        }

        if (const auto result = execute_shadow_pass_list(
                backend,
                render_data,
                render_data.point_shadow_passes,
                token,
                "ExecuteDirectionalShadowPassOperation (point)");
            !result)
        {
            return result;
        }

        if (const auto result = execute_shadow_pass_list(
                backend,
                render_data,
                render_data.spot_shadow_passes,
                token,
                "ExecuteDirectionalShadowPassOperation (spot)");
            !result)
        {
            return result;
        }

        if (const auto result = execute_shadow_pass_list(
                backend,
                render_data,
                render_data.area_shadow_passes,
                token,
                "ExecuteDirectionalShadowPassOperation (area)");
            !result)
        {
            return result;
        }

        if (render_data.directional_shadow_passes.empty() && render_data.point_shadow_passes.empty()
            && render_data.spot_shadow_passes.empty() && render_data.area_shadow_passes.empty())
            return {};

        return backend.set_viewport(render_data.frame.viewport);
    }

    Result ExecuteSkyboxPassOperation::prepare(RenderData&)
    {
        return {};
    }

    Result ExecuteSkyboxPassOperation::execute(
        IGraphicsBackend& backend,
        RenderData& render_data,
        const CancellationToken& token)
    {
        return execute_draw_list(
            backend,
            render_data,
            render_data.skybox_commands,
            GraphicsPassDesc {
                .clear_color = Color::BLACK,
                .clear_depth = 1.0F,
                .clear_stencil = 0U,
                .clear_flags = GraphicsClearFlags::COLOR_DEPTH,
                .debug_name = "Toybox Skybox Pass",
            },
            token,
            "ExecuteSkyboxPassOperation");
    }

    RenderOperationDebugInfo ExecuteOpaquePassOperation::get_debug_info() const
    {
        return make_pass_debug_info("Toybox Execute Opaque Pass Operation");
    }

    Result ExecuteOpaquePassOperation::prepare(RenderData&)
    {
        return {};
    }

    Result ExecuteOpaquePassOperation::execute(
        IGraphicsBackend& backend,
        RenderData& render_data,
        const CancellationToken& token)
    {
        return execute_draw_list(
            backend,
            render_data,
            render_data.opaque_commands,
            GraphicsPassDesc {
                .clear_color = Color::BLACK,
                .clear_depth = 1.0F,
                .clear_stencil = 0U,
                .clear_flags = render_data.has_skybox ? GraphicsClearFlags::DEPTH
                                                      : GraphicsClearFlags::COLOR_DEPTH,
                .debug_name = "Toybox Opaque Scene Pass",
            },
            token,
            "ExecuteOpaquePassOperation");
    }

    RenderOperationDebugInfo ExecuteAlphaCutoutPassOperation::get_debug_info() const
    {
        return make_pass_debug_info("Toybox Execute Alpha Cutout Pass Operation");
    }

    Result ExecuteAlphaCutoutPassOperation::prepare(RenderData&)
    {
        return {};
    }

    Result ExecuteAlphaCutoutPassOperation::execute(
        IGraphicsBackend& backend,
        RenderData& render_data,
        const CancellationToken& token)
    {
        return execute_draw_list(
            backend,
            render_data,
            render_data.alpha_cutout_commands,
            GraphicsPassDesc {
                .clear_color = Color::BLACK,
                .clear_depth = 1.0F,
                .clear_stencil = 0U,
                .clear_flags = GraphicsClearFlags::NONE,
                .debug_name = "Toybox Alpha Cutout Pass",
            },
            token,
            "ExecuteAlphaCutoutPassOperation");
    }

    RenderOperationDebugInfo ExecuteTransparentPassOperation::get_debug_info() const
    {
        return make_pass_debug_info("Toybox Execute Transparent Pass Operation");
    }

    Result ExecuteTransparentPassOperation::prepare(RenderData&)
    {
        return {};
    }

    Result ExecuteTransparentPassOperation::execute(
        IGraphicsBackend& backend,
        RenderData& render_data,
        const CancellationToken& token)
    {
        return execute_draw_list(
            backend,
            render_data,
            render_data.transparent_commands,
            GraphicsPassDesc {
                .clear_color = Color::BLACK,
                .clear_depth = 1.0F,
                .clear_stencil = 0U,
                .clear_flags = GraphicsClearFlags::NONE,
                .debug_name = "Toybox Transparent Pass",
            },
            token,
            "ExecuteTransparentPassOperation");
    }

    RenderOperationDebugInfo PresentOperation::get_debug_info() const
    {
        return make_pass_debug_info("Toybox Present Operation");
    }

    Result PresentOperation::prepare(RenderData&)
    {
        return {};
    }

    Result PresentOperation::execute(
        IGraphicsBackend& backend,
        RenderData& render_data,
        const CancellationToken& token)
    {
        auto& frame_data = render_data.frame;
        if (token && token.is_cancelled())
            return Result(false, "PresentOperation cancelled.");

        if (const auto result = ensure_frame_started(backend, render_data); !result)
            return result;

        if (frame_data.view_started)
        {
            if (const auto result = backend.end_view(); !result)
                return result;
            frame_data.view_started = false;
        }

        if (const auto result = backend.present(); !result)
            return result;

        if (frame_data.frame_started)
        {
            if (const auto result = backend.end_frame(); !result)
                return result;
            frame_data.frame_started = false;
        }

        return {};
    }
}
