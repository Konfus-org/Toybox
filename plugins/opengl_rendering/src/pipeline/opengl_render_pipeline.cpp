#include "opengl_render_pipeline.h"
#include "tbx/debugging/macros.h"
#include <any>

namespace opengl_rendering
{
    using namespace tbx;
    OpenGlRenderPipeline::OpenGlRenderPipeline() {}

    OpenGlRenderPipeline::~OpenGlRenderPipeline() noexcept
    {
        clear_operations();
    }

    void OpenGlRenderPipeline::execute(const std::any& payload)
    {
        // const auto* frame_context = std::any_cast<OpenGlRenderFrameContext>(&payload);
    }
}
