#pragma once
#include "opengl_render_pipeline.h"
#include "opengl_resources/opengl_buffers.h"
#include "opengl_resources/opengl_gbuffer.h"
#include "tbx/common/handle.h"
#include "tbx/common/uuid.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/messages.h"
#include "tbx/plugin_api/plugin.h"
#include <memory>
#include <vector>

namespace tbx::plugins
{
    /// <summary>Hosts the OpenGL rendering backend implementation.</summary>
    /// <remarks>Purpose: Owns OpenGL pipeline orchestration and frame submission.
    /// Ownership: Owns framebuffer resources and uniquely owns a lazily-created render pipeline.
    /// Thread Safety: Not thread-safe; use on the render thread.</remarks>
    class OpenGlRenderingPlugin final : public Plugin
    {
      public:
        void on_attach(IPluginHost& host) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;
        void on_recieve_message(Message& msg) override;

      private:
        /// <summary>Initializes OpenGL state and debug output.</summary>
        /// <remarks>Purpose: Configures core global render state once context is available.
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Call on the render thread with active OpenGL context.</remarks>
        void initialize_opengl() const;

        /// <summary>Updates the window viewport size used for presentation.</summary>
        /// <remarks>Purpose: Tracks output viewport dimensions for final blit operations.
        /// Ownership: Stores the provided value inside plugin-owned state.
        /// Thread Safety: Call on the render thread.</remarks>
        void set_viewport_size(const Size& viewport_size);

        /// <summary>Updates the internal render resolution and framebuffer size.</summary>
        /// <remarks>Purpose: Controls rasterization resolution independently from viewport size.
        /// Ownership: Resizes plugin-owned framebuffer attachments.
        /// Thread Safety: Call on the render thread.</remarks>
        void set_render_resolution(const Size& render_resolution);

      private:
        struct ResolvedSky final
        {
            RgbaColor clear_color = RgbaColor::black;
            MaterialInstance sky_material = {};
        };

        struct ResolvedPostProcessing final
        {
            std::vector<OpenGlPostProcessEffect> effects = {};
        };

        Uuid _window_id = invalid::uuid;
        Size _viewport_size = {0, 0};
        Size _render_resolution = {0, 0};
        bool _is_context_ready = false;
        OpenGlGBuffer _gbuffer = {};
        OpenGlFrameBuffer _lighting_framebuffer = {};
        OpenGlFrameBuffer _post_process_ping_framebuffer = {};
        OpenGlFrameBuffer _post_process_pong_framebuffer = {};
        std::unique_ptr<OpenGlRenderPipeline> _render_pipeline = nullptr;
        bool _is_sky_cache_valid = false;
        bool _cached_has_sky_component = false;
        uint64 _cached_sky_source_material_hash = 0U;
        std::shared_ptr<Material> _cached_sky_material = nullptr;
        ResolvedSky _cached_resolved_sky = {};
        ResolvedPostProcessing _cached_resolved_post_processing = {};
        std::vector<OpenGlDirectionalLightData> _frame_directional_lights = {};
        std::vector<OpenGlPointLightData> _frame_point_lights = {};
        std::vector<OpenGlSpotLightData> _frame_spot_lights = {};
    };
}
