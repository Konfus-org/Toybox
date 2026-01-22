#pragma once
#include "opengl_mesh.h"
#include "opengl_resource.h"
#include "opengl_shader.h"
#include "opengl_texture.h"
#include "tbx/app/application.h"
#include "tbx/common/int.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/messages.h"
#include "tbx/graphics/model.h"
#include "tbx/graphics/window.h"
#include "tbx/math/matrices.h"
#include "tbx/messages/observable.h"
#include "tbx/plugin_api/plugin.h"
#include <memory>
#include <unordered_map>

namespace tbx::plugins
{
    /// <summary>OpenGL rendering backend plugin.</summary>
    /// <remarks>Purpose: Implements GPU resource creation and frame rendering for OpenGL.
    /// Ownership: Owns OpenGL resources created through this backend.
    /// Thread Safety: Not thread-safe; call from the render thread.</remarks>
    class OpenGlRenderingPlugin final : public Plugin
    {
      public:
        /// <summary>Initializes the plugin with the host application.</summary>
        /// <remarks>Purpose: Stores the host for message dispatch and prepares state.
        /// Ownership: Does not take ownership of the host.
        /// Thread Safety: Called on the main thread during plugin attach.</remarks>
        void on_attach(Application& host) override;

        /// <summary>Handles incoming messages.</summary>
        /// <remarks>Purpose: Reacts to context readiness notifications.
        /// Ownership: Does not take ownership of messages.
        /// Thread Safety: Called on the dispatcher thread (typically main thread).</remarks>
        void on_recieve_message(Message& msg) override;

        /// <summary>Detaches the plugin from the host application.</summary>
        /// <remarks>Purpose: Releases OpenGL resources owned by the plugin.
        /// Ownership: Owns and releases OpenGL render targets.
        /// Thread Safety: Called on the main thread during plugin detach.</remarks>
        void on_detach() override;

        /// <summary>Updates rendering each frame.</summary>
        /// <remarks>Purpose: Renders all entities with model components using camera data when
        /// available.
        /// Ownership: Does not take ownership of any entity data.
        /// Thread Safety: Called on the main/render thread.</remarks>
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
        std::shared_ptr<OpenGlMesh> get_mesh(const Mesh& mesh);
        std::shared_ptr<OpenGlShader> get_shader(const Shader& shader);
        std::shared_ptr<OpenGlShaderProgram> get_shader_program(const Material& material);
        std::shared_ptr<OpenGlTexture> get_texture(const Texture& texture);
        void remove_window_state(const Uuid& window_id, bool try_release);

      private:
        bool _is_gl_ready = false;
        std::unordered_map<Uuid, Size> _window_sizes = {};
        Size _render_resolution = {1, 1};
        std::unordered_map<Uuid, std::shared_ptr<OpenGlMesh>> _meshes = {};
        std::unordered_map<Uuid, std::shared_ptr<OpenGlShader>> _shaders = {};
        std::unordered_map<Uuid, std::shared_ptr<OpenGlShaderProgram>> _shader_programs = {};
        std::unordered_map<Uuid, std::shared_ptr<OpenGlTexture>> _textures = {};
    };
}
