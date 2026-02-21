#pragma once
#include "opengl_render_pipeline.h"
#include "opengl_renderer_info.h"
#include "opengl_resources/opengl_buffers.h"
#include "opengl_resources/opengl_gbuffer.h"
#include "opengl_sky.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/common/result.h"
#include "tbx/common/uuid.h"
#include "tbx/ecs/entity_registry.h"
#include "tbx/graphics/messages.h"
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace tbx::plugins
{
    /// <summary>Owns shared OpenGL renderer state and frame execution.</summary>
    /// <remarks>Purpose: Manages GL initialization, scene extraction, and pipeline execution.
    /// Ownership: Owns render pipeline and all renderer-scoped GPU resource wrappers.
    /// Thread Safety: Not thread-safe; use on render thread only.</remarks>
    struct OpenGlRenderer final
    {
        using MakeCurrentSender = std::function<Result(const Uuid&)>;
        using PresentSender = std::function<Result(const Uuid&)>;

        OpenGlRenderer(
            GraphicsProcAddress loader,
            EntityRegistry& entity_registry,
            AssetManager& asset_manager,
            MakeCurrentSender make_current_sender,
            PresentSender present_sender);
        ~OpenGlRenderer() noexcept;

        /// <summary>Returns OpenGL implementation details discovered during
        /// initialization.</summary> <remarks>Purpose: Exposes diagnostics about active OpenGL
        /// runtime. Ownership: Returns reference to renderer-owned info data. Thread Safety: Read
        /// on render thread.</remarks>
        const OpenGlRendererInfo& get_info() const;

        /// <summary>Renders and presents one frame to the target window.</summary>
        /// <remarks>Purpose: Makes target context current, builds frame data, runs pipeline,
        /// presents. Ownership: Uses renderer-owned resources and non-owning host references.
        /// Thread Safety: Call on render thread only.</remarks>
        bool render(const Uuid& target_window_id);

        /// <summary>Updates current viewport size used for final presentation.</summary>
        /// <remarks>Purpose: Tracks current window output dimensions.
        /// Ownership: Stores value in renderer-owned state.
        /// Thread Safety: Call on render thread.</remarks>
        void set_viewport_size(const Size& viewport_size);

        /// <summary>Schedules a new render resolution to apply before next frame.</summary>
        /// <remarks>Purpose: Defers expensive framebuffer resize to render path.
        /// Ownership: Stores optional value in renderer-owned state.
        /// Thread Safety: Call on render thread.</remarks>
        void set_pending_render_resolution(const std::optional<Size>& pending_render_resolution);

      private:
        void initialize();
        void shutdown();
        void set_render_resolution(const Size& render_resolution);

      private:
        EntityRegistry* _entity_registry = nullptr;
        AssetManager* _asset_manager = nullptr;
        MakeCurrentSender _make_current_sender = nullptr;
        PresentSender _present_sender = nullptr;

        Size _viewport_size = {0, 0};
        Size _render_resolution = {0, 0};
        std::optional<Size> _pending_render_resolution = std::nullopt;
        OpenGlGBuffer _gbuffer = {};
        OpenGlFrameBuffer _lighting_framebuffer = {};
        OpenGlFrameBuffer _post_process_ping_framebuffer = {};
        OpenGlFrameBuffer _post_process_pong_framebuffer = {};
        std::unique_ptr<OpenGlRenderPipeline> _render_pipeline = nullptr;
        OpenGlSky _sky = {};
        std::vector<uint32> _shadow_map_texture_ids = {};
        OpenGlRendererInfo _info = {};
    };
}
