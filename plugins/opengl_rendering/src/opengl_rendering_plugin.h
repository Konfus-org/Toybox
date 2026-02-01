#pragma once
#include "opengl_mesh.h"
#include "opengl_resource.h"
#include "opengl_shader.h"
#include "opengl_texture.h"
#include "tbx/app/application.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/common/int.h"
#include "tbx/common/handle.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/messages.h"
#include "tbx/graphics/model.h"
#include "tbx/graphics/renderer.h"
#include "tbx/graphics/window.h"
#include "tbx/math/matrices.h"
#include "tbx/messages/observable.h"
#include "tbx/plugin_api/plugin.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>

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
        std::shared_ptr<Material> resolve_material_asset(
            AssetManager& asset_manager,
            const Handle& handle,
            const std::shared_ptr<Material>& fallback_material) const;
        void draw_mesh_with_material(
            AssetManager& asset_manager,
            const Mesh& mesh,
            const Material& material,
            const Mat4& model_matrix,
            const Mat4& view_projection,
            const Uuid& mesh_key);
        void draw_static_model(
            AssetManager& asset_manager,
            const StaticRenderData& static_data,
            const Mat4& entity_matrix,
            const Mat4& view_projection,
            const std::shared_ptr<Material>& fallback_material);
        void draw_model_parts(
            AssetManager& asset_manager,
            const Model& model,
            const Uuid& model_id,
            const Mat4& entity_matrix,
            const Mat4& view_projection,
            const std::shared_ptr<Material>& material_asset);
        void draw_model_part_recursive(
            AssetManager& asset_manager,
            const Model& model,
            const Uuid& model_id,
            const Mat4& parent_matrix,
            const Mat4& view_projection,
            const std::shared_ptr<Material>& material_asset,
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
        std::shared_ptr<OpenGlTexture> _default_texture = {};
    };
}
