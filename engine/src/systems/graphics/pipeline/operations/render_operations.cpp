#include "tbx/systems/graphics/pipeline/operations/render_operations.h"

namespace tbx
{
    RenderOperationDebugInfo BeginFrameOperation::get_debug_info() const
    {
        return RenderOperationDebugInfo {.debug_name = "Begin Frame", .category = "Frame"};
    }

    Result BeginFrameOperation::prepare(FrameData&)
    {
        return {};
    }

    Result BeginFrameOperation::execute(
        IGraphicsBackend& backend,
        FrameData& frame_data,
        const CancellationToken& token)
    {
        if (token.is_cancelled())
            return Result(false, "Begin frame cancelled.");

        auto result = backend.begin_frame(
            GraphicsFrameInfo {
                .output_window = frame_data.output_window,
                .render_resolution = frame_data.render_resolution,
                .output_resolution = frame_data.output_resolution,
            });
        if (!result)
            return result;

        result = backend.begin_view(frame_data.view);
        if (!result)
            return result;

        return backend.set_viewport(frame_data.view.viewport);
    }

    RenderPassListOperation::RenderPassListOperation(
        std::string debug_name,
        std::string category,
        PassListSelector pass_list_selector)
        : _debug_info(
              RenderOperationDebugInfo {
                  .debug_name = std::move(debug_name),
                  .category = std::move(category),
              })
        , _pass_list_selector(pass_list_selector)
    {
    }

    RenderOperationDebugInfo RenderPassListOperation::get_debug_info() const
    {
        return _debug_info;
    }

    Result RenderPassListOperation::prepare(FrameData&)
    {
        return {};
    }

    Result RenderPassListOperation::execute(
        IGraphicsBackend& backend,
        FrameData& frame_data,
        const CancellationToken& token)
    {
        if (_pass_list_selector == nullptr)
            return Result(false, "Render pass operation has no pass list selector.");

        for (const auto& render_pass : frame_data.*_pass_list_selector)
        {
            const auto result = _executor.execute(backend, render_pass, token);
            if (!result)
                return result;
        }

        return {};
    }

    RenderOperationDebugInfo EndFrameOperation::get_debug_info() const
    {
        return RenderOperationDebugInfo {.debug_name = "End Frame", .category = "Frame"};
    }

    Result EndFrameOperation::prepare(FrameData&)
    {
        return {};
    }

    Result EndFrameOperation::execute(
        IGraphicsBackend& backend,
        FrameData&,
        const CancellationToken& token)
    {
        if (token.is_cancelled())
            return Result(false, "End frame cancelled.");

        auto result = backend.end_view();
        if (!result)
            return result;

        result = backend.present();
        if (!result)
            return result;

        return backend.end_frame();
    }
}
