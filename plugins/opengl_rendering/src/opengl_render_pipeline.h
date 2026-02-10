#pragma once
#include "tbx/common/pipeline.h"

namespace tbx::plugins
{
    class OpenGlRenderPipeline final : public Pipeline
    {
        // TODO: Implement pipeline that has seperate operations/passes for drawing geometry,
        // post-processing, and shadow map generation. The pipeline makes a frame context that is
        // passed to each operation, which can read/write render state and resources from the
        // context. The pipeline is responsible for configuring OpenGL state before each operation
        // executes, and resetting state after execution to avoid leaking state between operations.
        // For now assume one camera and one render target, the pipeline should take these in via the frame context.
        // The pipeline should be a modern OpenGL implementation that uses VAOs, VBOs, and shader programs, and does not rely on deprecated fixed-function pipeline features.
        // The pipeline should be designed to allow for future extensions such as compute shader passes and ray tracing.
        // The pipeline should also manage a render cache that stores GPU resources like meshes and materials, and provides efficient lookup and reuse of these resources across frames.
        // The cache should be updated each frame to remove unused resources and avoid memory bloat, and should also handle resource creation and destruction in a way that minimizes GPU stalls and synchronization issues.
        // It should be easily made async in the future by allowing resource loading and compilation to happen on worker threads, and then safely transferring completed resources to the render thread for use in the pipeline.
    };
}