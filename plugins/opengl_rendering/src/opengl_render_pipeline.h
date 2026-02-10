#pragma once
#include "opengl_resources/opengl_buffers.h"
#include "opengl_resources/opengl_material.h"
#include "opengl_resources/opengl_mesh.h"
#include "opengl_resources/opengl_model.h"
#include "opengl_resources/opengl_shader.h"
#include "opengl_resources/opengl_texture.h"
#include "tbx/common/pipeline.h"
#include "tbx/common/uuid.h"
#include "tbx/graphics/camera.h"
#include "tbx/math/size.h"
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>

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
        /// Purpose: Stores the current frame context pointer for execute().
        /// </summary>
        /// <remarks>
        /// Ownership: Stores a non-owning pointer owned by the caller.
        /// Thread Safety: Not thread-safe; set on render thread before execute().
        /// </remarks>
        void set_frame_context(const OpenGlRenderFrameContext* frame_context);

        /// <summary>
        /// Purpose: Executes this operation using the active frame context.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of the frame context.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        void execute() override;

      protected:
        /// <summary>
        /// Purpose: Returns the currently assigned frame context.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a non-owning reference; caller does not own it.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        const OpenGlRenderFrameContext& get_frame_context() const;

        /// <summary>
        /// Purpose: Executes this operation using the provided frame context.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of the frame context.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        virtual void execute_with_frame_context(const OpenGlRenderFrameContext& frame_context) = 0;

      private:
        const OpenGlRenderFrameContext* _frame_context = nullptr;
    };

    /// <summary>
    /// Purpose: Executes OpenGL rendering work in a predefined operation order.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns render operations and cached resource maps by UUID.
    /// Thread Safety: Not thread-safe; configure and execute on the render thread.
    /// </remarks>
    class OpenGlRenderPipeline final : public Pipeline
    {
      public:
        /// <summary>
        /// Purpose: Configures the default OpenGL rendering operation sequence.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns created operations for the full pipeline lifetime.
        /// Thread Safety: Construct on the render thread.
        /// </remarks>
        OpenGlRenderPipeline();

        /// <summary>
        /// Purpose: Destroys the pipeline and clears any queued operations.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases owned operations and cached resources.
        /// Thread Safety: Destroy on the render thread.
        /// </remarks>
        ~OpenGlRenderPipeline() noexcept override;

        /// <summary>
        /// Purpose: Stores the frame context used by execute().
        /// </summary>
        /// <remarks>
        /// Ownership: Copies the context values and non-owning pointers.
        /// Thread Safety: Not thread-safe; configure on render thread.
        /// </remarks>
        void set_frame_context(const OpenGlRenderFrameContext& frame_context);

        /// <summary>
        /// Purpose: Executes the configured operation sequence using the current frame context.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of referenced frame resources.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        void execute() override;

        /// <summary>
        /// Purpose: Removes every cached OpenGL resource from all UUID maps.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases this pipeline's shared ownership of cached resources.
        /// Thread Safety: Not thread-safe; call on the render thread.
        /// </remarks>
        void clear_resource_caches();

        /// <summary>
        /// Purpose: Removes cached entries not present in the current live resource sets.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases shared ownership for pruned entries.
        /// Thread Safety: Not thread-safe; call on the render thread.
        /// </remarks>
        void trim_resource_caches(
            const std::unordered_set<Uuid>& live_mesh_ids,
            const std::unordered_set<Uuid>& live_texture_ids,
            const std::unordered_set<Uuid>& live_shader_program_ids,
            const std::unordered_set<Uuid>& live_material_ids,
            const std::unordered_set<Uuid>& live_model_ids);

      private:
        void assign_frame_context();
        void reset_frame_context();

        static void trim_cache(
            std::unordered_map<Uuid, std::shared_ptr<OpenGlMesh>>& cache,
            const std::unordered_set<Uuid>& live_ids);
        static void trim_cache(
            std::unordered_map<Uuid, std::shared_ptr<OpenGlTexture>>& cache,
            const std::unordered_set<Uuid>& live_ids);
        static void trim_cache(
            std::unordered_map<Uuid, std::shared_ptr<OpenGlShaderProgram>>& cache,
            const std::unordered_set<Uuid>& live_ids);
        static void trim_cache(
            std::unordered_map<Uuid, std::shared_ptr<OpenGlMaterial>>& cache,
            const std::unordered_set<Uuid>& live_ids);
        static void trim_cache(
            std::unordered_map<Uuid, std::shared_ptr<OpenGlModel>>& cache,
            const std::unordered_set<Uuid>& live_ids);

        std::optional<OpenGlRenderFrameContext> _current_frame_context = {};
        std::unordered_map<Uuid, std::shared_ptr<OpenGlMesh>> _meshes_by_id = {};
        std::unordered_map<Uuid, std::shared_ptr<OpenGlTexture>> _textures_by_id = {};
        std::unordered_map<Uuid, std::shared_ptr<OpenGlShaderProgram>> _shader_programs_by_id = {};
        std::unordered_map<Uuid, std::shared_ptr<OpenGlMaterial>> _materials_by_id = {};
        std::unordered_map<Uuid, std::shared_ptr<OpenGlModel>> _models_by_id = {};
    };
}
