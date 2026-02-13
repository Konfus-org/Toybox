#pragma once
#include "opengl_resources/opengl_buffers.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "tbx/common/handle.h"
#include "tbx/common/pipeline.h"
#include "tbx/ecs/entities.h"
#include "tbx/graphics/color.h"
#include "tbx/math/size.h"
#include <any>
#include <memory>
#include <span>
#include <vector>

namespace tbx::plugins
{
    /// <summary>
    /// Purpose: Captures the active camera entity used for frame visibility and matrix generation.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores a non-owning entity wrapper whose underlying registry must outlive frame
    /// execution.
    /// Thread Safety: Read-only on the render thread.
    /// </remarks>
    struct OpenGlCameraView final
    {
        /// <summary>
        /// Purpose: Active camera entity used to derive view and projection state.
        /// Ownership: Non-owning entity wrapper; registry and entity must outlive frame execution.
        /// Thread Safety: Read-only on the render thread.
        /// </summary>
        Entity camera_entity = {};

        /// <summary>
        /// Purpose: Static-mesh entities visible to the active camera and ready for rendering.
        /// Ownership: Stores non-owning entity wrappers into the owning entity registry.
        /// Thread Safety: Read-only on the render thread.
        /// </summary>
        std::vector<Entity> in_view_static_entities = {};

        /// <summary>
        /// Purpose: Dynamic-mesh entities visible to the active camera and ready for rendering.
        /// Ownership: Stores non-owning entity wrappers into the owning entity registry.
        /// Thread Safety: Read-only on the render thread.
        /// </summary>
        std::vector<Entity> in_view_dynamic_entities = {};
    };

    /// <summary>
    /// Purpose: Describes post-processing configuration for the current frame.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores value settings and a non-owning material handle reference.
    /// Thread Safety: Safe to read concurrently after construction.
    /// </remarks>
    struct OpenGlPostProcessEffect final
    {
        /// <summary>
        /// Purpose: Enables or disables this post-process effect.
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        bool is_enabled = false;

        /// <summary>
        /// Purpose: Runtime material data used to shade this fullscreen post-processing pass.
        /// Ownership: Owns parameter/texture values and a base material handle reference.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        MaterialInstance material = {};

        /// <summary>
        /// Purpose: Controls blend weight between source scene color and post-processed output.
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        float blend = 1.0f;
    };

    /// <summary>
    /// Purpose: Describes post-processing configuration for the current frame.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores value settings and a non-owning view over effect data owned by caller.
    /// Thread Safety: Safe to read concurrently after construction.
    /// </remarks>
    struct OpenGlPostProcessSettings final
    {
        /// <summary>
        /// Purpose: Enables or disables execution of post-processing.
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        bool is_enabled = false;

        /// <summary>
        /// Purpose: Ordered effects to execute from first to last.
        /// Ownership: Non-owning span; caller owns underlying storage.
        /// Thread Safety: Safe to read concurrently while underlying storage remains valid.
        /// </summary>
        std::span<const OpenGlPostProcessEffect> effects = {};
    };

    /// <summary>
    /// Purpose: Provides immutable per-frame data to OpenGL render operations.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores non-owning pointers and wrappers to scene resources; owners must outlive
    /// frame execution.
    /// Thread Safety: Safe to read concurrently after construction.
    /// </remarks>
    struct OpenGlRenderFrameContext
    {
        /// <summary>
        /// Purpose: Active camera and visible-entity list used by geometry rendering.
        /// Ownership: Value type owned by this context; internally uses non-owning entity
        /// wrappers.
        /// Thread Safety: Read-only on the render thread.
        /// </summary>
        OpenGlCameraView camera_view = {};

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
        /// Purpose: Clear color used when starting the geometry pass.
        /// Ownership: Value type owned by this context.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        RgbaColor clear_color = RgbaColor::black;

        /// <summary>
        /// Purpose: Optional sky runtime material used for skybox rendering in the geometry pass.
        /// Ownership: Stores a non-owning base material handle and runtime values by value.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        MaterialInstance sky_material = {};

        /// <summary>
        /// Purpose: Optional post-processing settings used for final fullscreen shading.
        /// Ownership: Value type owned by this context.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        OpenGlPostProcessSettings post_process = {};

        /// <summary>
        /// Purpose: Framebuffer used for geometry accumulation (G-buffer transition target).
        /// Ownership: Non-owning pointer; caller retains framebuffer lifetime.
        /// Thread Safety: Use on the render thread.
        /// </summary>
        OpenGlFrameBuffer* gbuffer_target = nullptr;

        /// <summary>
        /// Purpose: Framebuffer used for deferred lighting composition before post-processing.
        /// Ownership: Non-owning pointer; caller retains framebuffer lifetime.
        /// Thread Safety: Use on the render thread.
        /// </summary>
        OpenGlFrameBuffer* lighting_target = nullptr;

        /// <summary>
        /// Purpose: First intermediate framebuffer used for multi-pass post-processing.
        /// Ownership: Non-owning pointer; caller retains framebuffer lifetime.
        /// Thread Safety: Use on the render thread.
        /// </summary>
        OpenGlFrameBuffer* post_process_ping_target = nullptr;

        /// <summary>
        /// Purpose: Second intermediate framebuffer used for multi-pass post-processing.
        /// Ownership: Non-owning pointer; caller retains framebuffer lifetime.
        /// Thread Safety: Use on the render thread.
        /// </summary>
        OpenGlFrameBuffer* post_process_pong_target = nullptr;

        /// <summary>
        /// Purpose: Defines how the render target is scaled into the presentation target.
        /// Ownership: Value type owned by this context.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        OpenGlFrameBufferPresentMode present_mode = OpenGlFrameBufferPresentMode::ASPECT_FIT;

        /// <summary>
        /// Purpose: Framebuffer identifier used as the destination draw target for presentation.
        /// Ownership: Value type owned by this context.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        uint32 present_target_framebuffer_id = 0;
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
