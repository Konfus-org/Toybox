#pragma once
#include "tbx/common/pipeline.h"

namespace tbx::plugins
{
    class OpenGlRenderPipeline final : public Pipeline
    {
        // TODO: Implement a modern opengl render pipeline that has seperate operations/passes for
        // drawing geometry, post-processing, and shadow map generation. The pipeline makes a frame
        // context that is passed to each operation. For now assume one camera and one render
        // target, the pipeline should take these in via the frame context. The pipeline should be
        // designed be easily extensible, we may want compute shader passes and ray tracing in the
        // future. It should be easily made async in the future by allowing resource loading and
        // compilation to happen on worker threads, and then safely transferring completed resources
        // to the render thread for use in the pipeline.
    };
}