#include "tbx/systems/graphics/pipeline/view_uniform_operation.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/pipeline/render_frame_context.h"
#include "tbx/systems/math/matrices.h"

namespace tbx
{
    RenderOperationDebugInfo ViewUniformOperation::get_debug_info() const
    {
        auto debug_info = RenderOperationDebugInfo();
        debug_info.debug_name = "Toybox View Uniform Operation";
        debug_info.category = "Frame Setup";
        return debug_info;
    }

    struct ViewUniformBlock
    {
        Mat4 view_projection = Mat4(1.0F);
    };

    Result ViewUniformOperation::prepare(RenderFrameContext& context)
    {
        const auto block = ViewUniformBlock {.view_projection = context.view_projection};
        const auto data_size = static_cast<uint64>(sizeof(ViewUniformBlock));
        auto& backend = context.backend.get();

        if (!_buffer.is_valid())
        {
            if (const auto result = backend.upload_buffer(
                    GraphicsBufferDesc {
                        .usage = GraphicsBufferUsage::UNIFORM,
                        .size = data_size,
                        .is_dynamic = true,
                        .debug_name = "Toybox View Uniforms",
                    },
                    &block,
                    data_size,
                    _buffer);
                !result)
                return result;
        }
        else if (const auto result = backend.update_buffer(_buffer, &block, data_size, 0U); !result)
        {
            return result;
        }

        context.view_uniform_buffer = _buffer;
        return {};
    }

    Result ViewUniformOperation::execute(
        IGraphicsBackend& /*backend*/,
        const CancellationToken& /*token*/)
    {
        return {};
    }

    void ViewUniformOperation::release(IGraphicsBackend& backend)
    {
        if (_buffer.is_valid())
        {
            backend.unload(_buffer);
            _buffer = {};
        }
    }
}
