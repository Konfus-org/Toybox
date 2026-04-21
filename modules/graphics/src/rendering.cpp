#include "tbx/graphics/rendering.h"

namespace tbx
{
    static Result begin_frame_and_view(
        IGraphicsBackend& backend,
        const Window& output_window,
        const Camera& view,
        const Size& resolution)
    {
        const auto frame =
            GraphicsFrameInfo {
                .output_window = output_window,
                .render_resolution = resolution,
                .output_resolution = resolution,
            };
        if (const auto result = backend.begin_frame(frame); !result)
            return result;

        const auto graphics_view =
            GraphicsView {
                .camera = view,
                .viewport = Viewport {
                    .position = Vec2(0.0F),
                    .dimensions = resolution,
                },
            };
        if (const auto result = backend.begin_view(graphics_view); !result)
            return result;

        return backend.set_viewport(graphics_view.viewport);
    }

    static Result end_view_and_frame(IGraphicsBackend& backend)
    {
        if (const auto result = backend.end_view(); !result)
            return result;
        if (const auto result = backend.present(); !result)
            return result;

        return backend.end_frame();
    }

    Result Rendering::submit(
        IGraphicsBackend& backend,
        const Window& output_window,
        const Camera& view,
        const Size& resolution,
        const RenderGraph& scene) const
    {
        (void)scene;

        if (const auto result = begin_frame_and_view(backend, output_window, view, resolution);
            !result)
            return result;

        const auto pass =
            GraphicsPassDesc {
                .color_targets = {},
                .depth_stencil_target = {},
                .clear_color = Color::BLACK,
                .clear_depth = 1.0F,
                .clear_stencil = 0U,
                .clear_flags = GraphicsClearFlags::COLOR_DEPTH,
                .debug_name = "Frame Output",
            };
        if (const auto result = backend.begin_pass(pass); !result)
            return result;
        if (const auto result = backend.end_pass(); !result)
            return result;

        return end_view_and_frame(backend);
    }

    Result Rendering::submit(
        IGraphicsBackend& backend,
        const Window& output_window,
        const Camera& view,
        const Size& resolution,
        const GraphicsRenderPipeline& pipeline) const
    {
        if (const auto result = begin_frame_and_view(backend, output_window, view, resolution);
            !result)
            return result;
        if (const auto result = pipeline.execute(backend); !result)
            return result;

        return end_view_and_frame(backend);
    }
}
