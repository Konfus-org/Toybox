#include "tbx/graphics/rendering.h"

namespace tbx
{
    Result Rendering::submit(
        IGraphicsBackend& backend,
        const Window& output_window,
        const Camera& view,
        const Size& resolution,
        const RenderGraph& scene) const
    {
        (void)scene;

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

        if (const auto result = backend.set_viewport(graphics_view.viewport); !result)
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

        if (const auto result = backend.end_view(); !result)
            return result;
        if (const auto result = backend.present(); !result)
            return result;

        return backend.end_frame();
    }
}
