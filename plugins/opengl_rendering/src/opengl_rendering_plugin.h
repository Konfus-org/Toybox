#pragma once
#include "opengl_frame_info.h"
#include "opengl_render_cache.h"
#include "opengl_render_target.h"
#include "opengl_shadow_map_target.h"
#include "opengl_resource.h"
#include "tbx/app/application.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/common/handle.h"
#include "tbx/common/int.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/messages.h"
#include "tbx/graphics/renderer.h"
#include "tbx/graphics/window.h"
#include "tbx/messages/observable.h"
#include "tbx/plugin_api/plugin.h"
#include <memory>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tbx::plugins
{
    /// <summary>OpenGL rendering backend plugin.</summary>
    /// <remarks>
    /// Purpose: Implements GPU resource creation and frame rendering for OpenGL.
    /// Ownership: Owns OpenGL resources created through this backend.
    /// Thread Safety: Not thread-safe; call from the render thread.
    /// </remarks>
    class OpenGlRenderingPlugin final : public Plugin
    {
      public:
        void on_attach(IPluginHost& host) override;
        void on_recieve_message(Message& msg) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;

      private:
        void handle_window_ready(WindowContextReadyEvent& event);
        void handle_window_open_changed(PropertyChangedEvent<Window, bool>& event);
        void handle_window_resized(PropertyChangedEvent<Window, Size>& event);
        void handle_resolution_changed(PropertyChangedEvent<AppSettings, Size>& event);
        void initialize_opengl() const;
        Size get_effective_resolution(const Size& window_size) const;
        void build_frame_buffer(OpenGlFrameBufferInfo& frame, const Size& window_size);
        void build_frame_lighting(OpenGlFrameBufferInfo& frame);
        void build_frame_cameras(OpenGlFrameBufferInfo& frame, const Size& render_resolution);
        void build_draw_requests_for_camera(OpenGlFrameCameraInfo& camera);
        void render_directional_shadow_map(OpenGlFrameBufferInfo& frame);
        void draw_frame(OpenGlFrameBufferInfo& frame);
        void draw_mesh_with_material(
            OpenGlFrameCameraInfo& camera,
            OpenGlFrameLightingInfo& lighting,
            const Uuid& mesh_key,
            const OpenGlMaterial& material,
            std::span<const ShaderUniform> material_overrides,
            const Mat4& model_matrix,
            const Mat4& view_projection,
            const Vec3& camera_position);
        void remove_window_state(const Uuid& window_id, bool try_release);

      private:
        bool _is_gl_ready = false;
        Size _render_resolution = {1, 1};
        std::unordered_map<Uuid, Size> _window_sizes = {};
        std::unordered_map<Uuid, OpenGlRenderTarget> _render_targets = {};
        std::unordered_map<Uuid, OpenGlShadowMapTarget> _directional_shadow_targets = {};
        OpenGlRenderCache _cache = {};
        GlIdProvider _id_provider = {};
        std::shared_ptr<OpenGlShaderProgram> _directional_shadow_program = {};
        OpenGlFrameBufferInfo _frame = {};
    };
}
