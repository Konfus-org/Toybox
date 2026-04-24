#include "tbx/systems/graphics/rendering.h"
#include <memory>

namespace tbx
{
    static Result begin_frame_and_view(IGraphicsBackend& backend, const RenderViewSubmission& view)
    {
        const auto frame = GraphicsFrameInfo {
            .output_window = view.output_window,
            .render_resolution = view.resolution,
            .output_resolution = view.resolution,
        };
        if (const auto result = backend.begin_frame(frame); !result)
            return result;

        const auto graphics_view = GraphicsView {
            .camera = view.camera,
            .viewport =
                Viewport {
                    .position = Vec2(0.0F),
                    .dimensions = view.resolution,
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

    Result Rendering::initialize(IGraphicsBackend& backend, const GraphicsSettings& settings)
    {
        if (_is_initialized && _backend.has_value() && &_backend->get() == &backend)
            return {};

        if (_backend.has_value() && &_backend->get() != &backend)
        {
            _backend->get().wait_for_idle();
            _pipeline.reset();
            _backend = std::nullopt;
            _is_initialized = false;
        }

        if (const auto result = backend.initialize(settings); !result)
            return result;

        rebuild_pipeline(backend);
        _backend = std::ref(backend);
        _is_initialized = true;
        return {};
    }

    void Rendering::setup_default_pipeline()
    {
        if (!_pipeline)
            return;

        _pipeline->clear();
        _pipeline->add_pass_operation(
            GraphicsRenderPass {
                .pass =
                    GraphicsPassDesc {
                        .clear_color = Color::BLACK,
                        .clear_depth = 1.0F,
                        .clear_stencil = 0U,
                        .clear_flags = GraphicsClearFlags::COLOR_DEPTH,
                        .debug_name = "Toybox Geometry Pass",
                    },
            });
    }

    Result Rendering::submit(IGraphicsBackend& backend, const RenderViewSubmission& view) const
    {
        if (!_is_initialized || !_pipeline || !_backend.has_value() || &_backend->get() != &backend)
            return Result(
                false,
                "Rendering::initialize must be called with this backend before submit.");

        if (const auto result = begin_frame_and_view(backend, view); !result)
            return result;
        if (const auto result = _pipeline->execute(); !result)
            return result;

        return end_view_and_frame(backend);
    }

    Result Rendering::submit(
        IGraphicsBackend& backend,
        const std::vector<RenderViewSubmission>& views) const
    {
        for (const auto& view : views)
        {
            if (const auto result = submit(backend, view); !result)
                return result;
        }

        return {};
    }

    const GraphicsRenderPipeline& Rendering::get_pipeline() const
    {
        return *_pipeline;
    }

    void Rendering::rebuild_pipeline(IGraphicsBackend& backend)
    {
        _pipeline = std::make_unique<GraphicsRenderPipeline>(backend);
        setup_default_pipeline();
    }
}
