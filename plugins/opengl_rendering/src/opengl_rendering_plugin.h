#pragma once
#include "opengl_material.h"
#include "opengl_mesh.h"
#include "opengl_model.h"
#include "opengl_resource.h"
#include "opengl_shader.h"
#include "opengl_texture.h"
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
        OpenGlModel* get_cached_model(
            AssetManager& asset_manager,
            const Handle& handle);
        OpenGlMaterial* get_cached_material(
            AssetManager& asset_manager,
            const Handle& handle,
            const std::shared_ptr<Material>& fallback_material);
        OpenGlMaterial* get_cached_fallback_material(
            AssetManager& asset_manager,
            const std::shared_ptr<Material>& fallback_material);
        bool try_build_gl_material(
            AssetManager& asset_manager,
            const Material& source_material,
            OpenGlMaterial& out_material);
        std::shared_ptr<OpenGlShaderProgram> ensure_shader_program(
            AssetManager& asset_manager,
            Handle& shader_handle);
        std::shared_ptr<OpenGlMesh> get_cached_mesh(const Uuid& mesh_key) const;
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
        std::shared_ptr<OpenGlTexture> get_default_texture();
        std::shared_ptr<OpenGlMesh> get_mesh(const Mesh& mesh, const Uuid& mesh_key);
        std::shared_ptr<OpenGlShader> get_shader(const ShaderSource& shader, const Uuid& shader_key);
        std::shared_ptr<OpenGlShaderProgram> get_shader_program(
            const Uuid& shader_id,
            const Shader& shader);
        std::shared_ptr<OpenGlTexture> get_texture(const Uuid& texture_id, const Texture& texture);
        void remove_window_state(const Uuid& window_id, bool try_release);

      private:
        bool _is_gl_ready = false;
        Size _render_resolution = {1, 1};
        std::unordered_map<Uuid, Size> _window_sizes = {};
        std::unordered_map<Uuid, std::shared_ptr<OpenGlMesh>> _meshes = {};
        std::unordered_map<Uuid, std::shared_ptr<OpenGlShader>> _shaders = {};
        std::unordered_map<Uuid, std::shared_ptr<OpenGlShaderProgram>> _shader_programs = {};
        std::unordered_map<Uuid, std::shared_ptr<OpenGlTexture>> _textures = {};
        std::unordered_map<Uuid, OpenGlModel> _models = {};
        std::unordered_map<Uuid, OpenGlMaterial> _materials = {};
        OpenGlMaterial _fallback_material = {};
        bool _has_fallback_material = false;
        std::shared_ptr<OpenGlTexture> _default_texture = {};
    };
}
