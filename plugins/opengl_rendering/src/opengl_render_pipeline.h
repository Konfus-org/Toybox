#pragma once
#include "opengl_resources/opengl_buffers.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "tbx/common/pipeline.h"
#include "tbx/graphics/camera.h"
#include "tbx/math/size.h"
#include <any>
#include <memory>

namespace tbx::plugins
{
    /// <summary>
    /// Purpose: Provides immutable per-frame data to OpenGL render operations.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores non-owning pointers to scene resources; owner must outlive frame
    /// execution. Thread Safety: Safe to read concurrently after construction.
    /// </remarks>
    struct OpenGlRenderFrameContext
    {
        /// <summary>
        /// Purpose: Active camera used to build view and projection matrices for the frame.
        /// Ownership: Non-owning pointer; must remain valid during pipeline execution.
        /// Thread Safety: Read-only on the render thread.
        /// </summary>
        const Camera* camera = nullptr;

        /// <summary>
        /// Purpose: Internal render resolution used for scene rasterization.
        /// Ownership: Value type owned by this context.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        Size render_resolution = {};

        /// <summary>
        /// Purpose: Output viewport size used for final presentation.
        /// Ownership: Value type owned by this context.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        Size viewport_size = {};

        /// <summary>
        /// Purpose: Framebuffer used for scene rendering before post-processing/present.
        /// Ownership: Non-owning pointer; caller retains framebuffer lifetime.
        /// Thread Safety: Use on the render thread.
        /// </summary>
        OpenGlFrameBuffer* render_target = nullptr;
    };

    /// <summary>
    /// Purpose: Represents a render-thread operation that consumes a per-frame OpenGL context.
    /// </summary>
    /// <remarks>
    /// Ownership: Owned by OpenGlRenderPipeline through unique pointers.
    /// Thread Safety: Not thread-safe; execute and mutate on the render thread.
    /// </remarks>
    class OpenGlRenderOperation : public PipelineOperation
    {
      public:
        /// <summary>
        /// Purpose: Executes this operation using a payload-backed frame context.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of frame context resources.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        void execute(const std::any& payload) override;

      protected:
        /// <summary>
        /// Purpose: Executes this operation using the provided frame context.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of the frame context.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        virtual void execute_with_frame_context(const OpenGlRenderFrameContext& frame_context) = 0;
    };

    /// <summary>
    /// Purpose: Executes OpenGL rendering work in a predefined operation order.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns render operations and an OpenGL resource manager.
    /// Thread Safety: Not thread-safe; configure and execute on the render thread.
    /// </remarks>
    class OpenGlRenderPipeline final : public Pipeline
    {
      public:
        /// <summary>
        /// Purpose: Configures the default OpenGL rendering operation sequence.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns created operations and the resource manager.
        /// Thread Safety: Construct on the render thread.
        /// </remarks>
        explicit OpenGlRenderPipeline(AssetManager& asset_manager);

        /// <summary>
        /// Purpose: Destroys the pipeline and clears any queued operations.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases owned operations and cached resources.
        /// Thread Safety: Destroy on the render thread.
        /// </remarks>
        ~OpenGlRenderPipeline() noexcept override;

        /// <summary>
        /// Purpose: Executes the configured operation sequence using payload frame data.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of payload resources.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        void execute(const std::any& payload) override;

        /// <summary>
        /// Purpose: Removes every cached OpenGL resource from the resource manager.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases this pipeline's shared ownership of cached resources.
        /// Thread Safety: Not thread-safe; call on the render thread.
        /// </remarks>
        void clear_resource_caches();

      private:
        OpenGlResourceManager _resource_manager;
    };
}
