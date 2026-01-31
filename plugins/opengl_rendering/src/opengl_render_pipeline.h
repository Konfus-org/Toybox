#pragma once
#include "opengl_mesh.h"
#include "opengl_shader.h"
#include "opengl_texture.h"
#include "tbx/common/pipeline.h"
#include "tbx/common/uuid.h"
#include "tbx/graphics/window.h"
#include <memory>
#include <unordered_map>

namespace tbx
{
    class AssetManager;
    class EntityManager;
    class IMessageDispatcher;
}

namespace tbx::plugins
{

    /// <summary>
    /// Purpose: Stores per-frame OpenGL render state for pipeline operations.
    /// </summary>
    /// <remarks>
    /// Ownership: Owned by the pipeline and passed as a non-owning reference to operations.
    /// Thread Safety: Not thread-safe; mutate on the render thread.
    /// </remarks>
    struct FrameContext final
    {
        Uuid window_id = {};
        Size window_size = {1, 1};
        Size render_resolution = {1, 1};
        bool is_frame_valid = true;
    };

    /// <summary>
    /// Purpose: Caches OpenGL resources for reuse across frames.
    /// </summary>
    /// <remarks>
    /// Ownership: Owned by the pipeline and shared with operations via non-owning references.
    /// Thread Safety: Not thread-safe; access on the render thread.
    /// </remarks>
    struct OpenGlResourceCache final
    {
        std::unordered_map<Uuid, std::shared_ptr<OpenGlMesh>> meshes = {};
        std::unordered_map<Uuid, std::shared_ptr<OpenGlShader>> shaders = {};
        std::unordered_map<Uuid, std::shared_ptr<OpenGlShaderProgram>> shader_programs = {};
        std::unordered_map<Uuid, std::shared_ptr<OpenGlTexture>> textures = {};
        std::shared_ptr<OpenGlTexture> default_texture = {};
    };

    /// <summary>
    /// Purpose: Executes ordered OpenGL rendering operations as a reusable pipeline.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the operation pipeline; operations retain non-owning references to services.
    /// Thread Safety: Not thread-safe; invoke from the render thread.
    /// </remarks>
    class OpenGlRenderPipeline final
    {
      public:
        /// <summary>
        /// Purpose: Constructs a pipeline bound to the given render services.
        /// </summary>
        /// <remarks>
        /// Ownership: Passes non-owning references to the supplied services into operations.
        /// Thread Safety: Not thread-safe; call from the render thread.
        /// </remarks>
        OpenGlRenderPipeline(
            IMessageDispatcher& dispatcher,
            EntityManager& entity_manager,
            AssetManager& asset_manager,
            const Size& render_resolution);

        /// <summary>
        /// Purpose: Updates the desired render resolution for future frames.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of the resolution value.
        /// Thread Safety: Not thread-safe; call from the render thread.
        /// </remarks>
        void set_render_resolution(const Size& resolution);

        /// <summary>
        /// Purpose: Executes a full frame for the specified window.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not assume ownership of window resources.
        /// Thread Safety: Not thread-safe; call from the render thread.
        /// </remarks>
        void execute_frame(const Uuid& window_id, const Size& window_size);

      private:
        Size get_effective_resolution(const Size& window_size) const;

        Size _render_resolution = {1, 1};
        FrameContext _context = {};
        std::unique_ptr<Pipeline> _frame_pipeline = {};
        OpenGlResourceCache _cache = {};
    };
}
