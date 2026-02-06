#pragma once
#include "opengl_render_cache.h"
#include "opengl_render_target.h"
#include "opengl_resource.h"
#include "tbx/app/application.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/common/int.h"
#include "tbx/common/handle.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/messages.h"
#include "tbx/graphics/renderer.h"
#include "tbx/graphics/window.h"
#include "tbx/math/matrices.h"
#include "tbx/messages/observable.h"
#include "tbx/plugin_api/plugin.h"
#include <memory>
#include <unordered_map>

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
        void draw_models(const Mat4& view_projection);
        void draw_models_for_cameras(const Size& window_size);
        void draw_mesh_with_material(
            const Uuid& mesh_key,
            const OpenGlMaterial& material,
            const Mat4& model_matrix,
            const Mat4& view_projection);
        void draw_static_model(
            AssetManager& asset_manager,
            const StaticRenderData& static_data,
            const Mat4& entity_matrix,
            const Mat4& view_projection,
            const std::shared_ptr<Material>& fallback_material);
        void draw_model_parts(
            const OpenGlModel& model,
            const Mat4& entity_matrix,
            const Mat4& view_projection,
            const OpenGlMaterial* override_material,
            const OpenGlMaterial& fallback_material);
        void draw_model_part_recursive(
            const OpenGlModel& model,
            const Mat4& parent_matrix,
            const Mat4& view_projection,
            const OpenGlMaterial* override_material,
            const OpenGlMaterial& fallback_material,
            uint32 part_index);
        void draw_procedural_meshes(
            AssetManager& asset_manager,
            const ProceduralData& procedural_data,
            const Mat4& entity_matrix,
            const Mat4& view_projection,
            const std::shared_ptr<Material>& fallback_material);
        void remove_window_state(const Uuid& window_id, bool try_release);

      private:
        bool _is_gl_ready = false;
        Size _render_resolution = {1, 1};
        std::unordered_map<Uuid, Size> _window_sizes = {};
        std::unordered_map<Uuid, OpenGlRenderTarget> _render_targets = {};
        OpenGlRenderCache _cache = {};
        GlIdProvider _id_provider = {};
    };
}
