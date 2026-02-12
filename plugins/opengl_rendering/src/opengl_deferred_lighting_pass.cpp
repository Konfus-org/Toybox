#include "opengl_deferred_lighting_pass.h"
#include "tbx/debugging/macros.h"

namespace tbx::plugins
{
    void OpenGlDeferredLightingPass::execute(const OpenGlRenderFrameContext& frame_context) const
    {
        TBX_ASSERT(
            frame_context.gbuffer_target != nullptr,
            "OpenGL rendering: deferred pass requires a geometry target.");
        TBX_ASSERT(
            frame_context.lighting_target != nullptr,
            "OpenGL rendering: deferred pass requires a lighting target.");

        frame_context.gbuffer_target->preset(
            frame_context.lighting_target->get_framebuffer_id(),
            frame_context.lighting_target->get_resolution(),
            OpenGlFrameBufferPresentMode::STRETCH);
    }
}
