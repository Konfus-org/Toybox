#include "opengl_rendering_plugin.h"
#include "tbx/app/application.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/assets/builtin_assets.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/model.h"
#include "tbx/math/matrices.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"
#include <glad/glad.h>
#ifdef USING_SDL3
    #include <SDL3/SDL.h>
#endif
#include <algorithm>
#include <string>
#include <vector>

namespace tbx::plugins
{
    static std::string to_uniform_name(const std::string& material_name)
    {
        if (material_name.size() >= 2U && material_name[0] == 'u' && material_name[1] == '_')
        {
            return material_name;
        }
        return "u_" + material_name;
    }

    static void ensure_default_shader_textures(OpenGlMaterial& material)
    {
        if (material.shader_programs.size() != 1U
            || material.shader_programs.front() != default_shader.id)
        {
            return;
        }

        std::string diffuse_name = to_uniform_name("diffuse");
        std::string normal_name = to_uniform_name("normal");

        OpenGlMaterialTexture diffuse = {.name = diffuse_name};
        OpenGlMaterialTexture normal = {.name = normal_name};

        for (const auto& texture : material.textures)
        {
            if (texture.name == diffuse_name)
            {
                diffuse = texture;
            }
            else if (texture.name == normal_name)
            {
                normal = texture;
            }
        }

        std::vector<OpenGlMaterialTexture> reordered = {};
        reordered.reserve(material.textures.size() + 2U);
        reordered.push_back(std::move(diffuse));
        reordered.push_back(std::move(normal));

        for (const auto& texture : material.textures)
        {
            if (texture.name == diffuse_name || texture.name == normal_name)
            {
                continue;
            }
            reordered.push_back(texture);
        }

        material.textures = std::move(reordered);
    }

    static GLuint compile_shader(GLenum type, const char* source)
    {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint status = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (status != GL_TRUE)
        {
            GLchar log[1024] = {};
            GLsizei log_length = 0;
            glGetShaderInfoLog(shader, static_cast<GLsizei>(sizeof(log)), &log_length, log);
            TBX_TRACE_ERROR("OpenGL rendering: shader compile failed: {}", log);
            glDeleteShader(shader);
            return 0U;
        }

        return shader;
    }

    static GLuint link_program(GLuint vertex_shader, GLuint fragment_shader)
    {
        GLuint program = glCreateProgram();
        glAttachShader(program, vertex_shader);
        glAttachShader(program, fragment_shader);
        glLinkProgram(program);

        GLint status = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &status);
        if (status != GL_TRUE)
        {
            GLchar log[1024] = {};
            GLsizei log_length = 0;
            glGetProgramInfoLog(program, static_cast<GLsizei>(sizeof(log)), &log_length, log);
            TBX_TRACE_ERROR("OpenGL rendering: program link failed: {}", log);
            glDeleteProgram(program);
            return 0U;
        }

        return program;
    }

    template <typename TRenderTarget>
    static void destroy_render_target(TRenderTarget& target)
    {
        if (target.depth_stencil != 0U)
        {
            const auto id = static_cast<GLuint>(target.depth_stencil);
            glDeleteRenderbuffers(1, &id);
            target.depth_stencil = 0U;
        }

        if (target.color_texture != 0U)
        {
            const auto id = static_cast<GLuint>(target.color_texture);
            glDeleteTextures(1, &id);
            target.color_texture = 0U;
        }

        if (target.framebuffer != 0U)
        {
            const auto id = static_cast<GLuint>(target.framebuffer);
            glDeleteFramebuffers(1, &id);
            target.framebuffer = 0U;
        }

        target.size = {0U, 0U};
    }

    template <typename TRenderTarget>
    static bool try_create_render_target(TRenderTarget& target, const Size& size)
    {
        destroy_render_target(target);

        if (size.width == 0U || size.height == 0U)
        {
            return false;
        }

        GLuint framebuffer = 0U;
        GLuint color_texture = 0U;
        GLuint depth_stencil = 0U;

        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

        glGenTextures(1, &color_texture);
        glBindTexture(GL_TEXTURE_2D, color_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            static_cast<GLsizei>(size.width),
            static_cast<GLsizei>(size.height),
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            nullptr);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            color_texture,
            0);

        glGenRenderbuffers(1, &depth_stencil);
        glBindRenderbuffer(GL_RENDERBUFFER, depth_stencil);
        glRenderbufferStorage(
            GL_RENDERBUFFER,
            GL_DEPTH24_STENCIL8,
            static_cast<GLsizei>(size.width),
            static_cast<GLsizei>(size.height));
        glFramebufferRenderbuffer(
            GL_FRAMEBUFFER,
            GL_DEPTH_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER,
            depth_stencil);

        auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            glDeleteRenderbuffers(1, &depth_stencil);
            glDeleteTextures(1, &color_texture);
            glDeleteFramebuffers(1, &framebuffer);
            return false;
        }

        target.framebuffer = static_cast<uint32>(framebuffer);
        target.color_texture = static_cast<uint32>(color_texture);
        target.depth_stencil = static_cast<uint32>(depth_stencil);
        target.size = size;
        return true;
    }

    // Build a model matrix from an entity transform.
    static Mat4 build_model_matrix(const Transform& transform)
    {
        Mat4 translation = translate(transform.position);
        Mat4 rotation = quaternion_to_mat4(transform.rotation);
        Mat4 scaling = scale(transform.scale);
        return translation * rotation * scaling;
    }

    // Compute a cache key for procedural meshes using a stable batch id and mesh index.
    static Uuid make_procedural_mesh_key(const Uuid& batch_id, uint32 mesh_index)
    {
        return Uuid::combine(batch_id, mesh_index);
    }

    // Compute a cache key for asset-backed model meshes.
    static Uuid make_model_mesh_key(const Uuid& model_id, uint32 part_index)
    {
        return Uuid::combine(model_id, part_index);
    }

    static void GLAPIENTRY gl_message_callback(
        GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        const GLchar* message,
        const void* user_param)
    {
        (void)source;
        (void)type;
        (void)id;
        (void)length;
        (void)user_param;

        switch (severity)
        {
            case GL_DEBUG_SEVERITY_HIGH:
                TBX_ASSERT(false, "OpenGL callback: {}", message);
                break;
            case GL_DEBUG_SEVERITY_MEDIUM:
                TBX_TRACE_ERROR("OpenGL callback: {}", message);
                break;
            case GL_DEBUG_SEVERITY_LOW:
                TBX_TRACE_WARNING("OpenGL callback: {}", message);
                break;
            case GL_DEBUG_SEVERITY_NOTIFICATION:
                TBX_TRACE_INFO("OpenGL callback: {}", message);
                break;
            default:
                TBX_TRACE_WARNING("OpenGL callback: {}", message);
                break;
        }
    }

    void OpenGlRenderingPlugin::on_attach(IPluginHost& host)
    {
        _render_resolution = host.get_settings().resolution.value;
    }

    void OpenGlRenderingPlugin::on_detach()
    {
        for (auto& entry : _render_targets)
        {
            Uuid window_id = entry.first;
            auto result = send_message<WindowMakeCurrentRequest>(window_id);
            if (result)
            {
                destroy_render_target(entry.second);
                destroy_present_pipeline(entry.second.present);
            }
        }

        _is_gl_ready = false;
        _window_sizes.clear();
        _render_targets.clear();
        _meshes.clear();
        _shaders.clear();
        _shader_programs.clear();
        _textures.clear();
        _models.clear();
        _materials.clear();
        _fallback_material = {};
        _has_fallback_material = false;
        _default_texture.reset();
    }

    void OpenGlRenderingPlugin::on_update(const DeltaTime&)
    {
        if (!_is_gl_ready || _window_sizes.empty())
        {
            return;
        }

        // Track windows that fail to render so their state can be dropped.
        std::vector<Uuid> windows_to_remove = {};
        windows_to_remove.reserve(_window_sizes.size());

        for (const auto& entry : _window_sizes)
        {
            Uuid window_id = entry.first;
            const Size& window_size = entry.second;
            Size render_resolution = get_effective_resolution(window_size);

            // Bind the window context before issuing any GL commands.
            auto result = send_message<WindowMakeCurrentRequest>(window_id);
            if (!result)
            {
                TBX_TRACE_ERROR(
                    "OpenGL rendering: failed to make window current: {}",
                    result.get_report());
                windows_to_remove.push_back(window_id);
                continue;
            }

            bool should_scale_to_window = render_resolution.width != window_size.width
                                          || render_resolution.height != window_size.height;

            if (should_scale_to_window)
            {
                auto& target = _render_targets[window_id];
                if (target.size.width != render_resolution.width
                    || target.size.height != render_resolution.height)
                {
                    if (!try_create_render_target(target, render_resolution))
                    {
                        TBX_TRACE_WARNING(
                            "OpenGL rendering: Failed to create render target {}x{} for window {}.",
                            render_resolution.width,
                            render_resolution.height,
                            window_id.value);
                    }
                }

                if (target.framebuffer != 0U)
                {
                    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(target.framebuffer));
                    glViewport(
                        0,
                        0,
                        static_cast<GLsizei>(render_resolution.width),
                        static_cast<GLsizei>(render_resolution.height));
                    glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Fixed 255 to 1.0f
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                    // Draw all scene cameras to the offscreen target at the logical resolution.
                    draw_models_for_cameras(render_resolution);

                    // Present the scaled image to the window backbuffer via a shader, avoiding
                    // glBlitFramebuffer scaling (which can be invalid with multisampled defaults).
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glViewport(
                        0,
                        0,
                        static_cast<GLsizei>(window_size.width),
                        static_cast<GLsizei>(window_size.height));
                    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                    glClear(GL_COLOR_BUFFER_BIT);

                    auto& pipeline = target.present;
                    if (pipeline.program == 0U)
                    {
                        initialize_present_pipeline(pipeline);
                    }

                    if (pipeline.program != 0U)
                    {
                        float x_scale = static_cast<float>(window_size.width)
                                        / static_cast<float>(render_resolution.width);
                        float y_scale = static_cast<float>(window_size.height)
                                        / static_cast<float>(render_resolution.height);
                        float scale = std::min(x_scale, y_scale);

                        int scaled_width = std::max(
                            1,
                            static_cast<int>(static_cast<float>(render_resolution.width) * scale));
                        int scaled_height = std::max(
                            1,
                            static_cast<int>(static_cast<float>(render_resolution.height) * scale));

                        int x_offset =
                            std::max(0, (static_cast<int>(window_size.width) - scaled_width) / 2);
                        int y_offset =
                            std::max(0, (static_cast<int>(window_size.height) - scaled_height) / 2);

                        bool was_depth_test_enabled = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
                        bool was_blend_enabled = glIsEnabled(GL_BLEND) == GL_TRUE;

                        glDisable(GL_DEPTH_TEST);
                        glDisable(GL_BLEND);

                        glUseProgram(static_cast<GLuint>(pipeline.program));
                        glBindVertexArray(static_cast<GLuint>(pipeline.vertex_array));
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(target.color_texture));
                        if (pipeline.texture_uniform >= 0)
                        {
                            glUniform1i(pipeline.texture_uniform, 0);
                        }

                        glViewport(x_offset, y_offset, scaled_width, scaled_height);
                        glDrawArrays(GL_TRIANGLES, 0, 3);

                        glBindTexture(GL_TEXTURE_2D, 0);
                        glBindVertexArray(0);
                        glUseProgram(0);

                        if (was_blend_enabled)
                        {
                            glEnable(GL_BLEND);
                        }
                        if (was_depth_test_enabled)
                        {
                            glEnable(GL_DEPTH_TEST);
                        }

                        glViewport(
                            0,
                            0,
                            static_cast<GLsizei>(window_size.width),
                            static_cast<GLsizei>(window_size.height));
                    }
                }
                else
                {
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glViewport(
                        0,
                        0,
                        static_cast<GLsizei>(window_size.width),
                        static_cast<GLsizei>(window_size.height));
                    glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Fixed 255 to 1.0f
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    draw_models_for_cameras(window_size);
                }
            }
            else
            {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glViewport(
                    0,
                    0,
                    static_cast<GLsizei>(render_resolution.width),
                    static_cast<GLsizei>(render_resolution.height));
                glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Fixed 255 to 1.0f
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                // Draw all scene cameras to the current framebuffer.
                draw_models_for_cameras(render_resolution);
            }

            // Present the rendered frame to the window.
            glFlush();
            auto present_result = send_message<WindowPresentRequest>(window_id);
            if (!present_result)
            {
                TBX_TRACE_ERROR(
                    "OpenGL rendering: failed to present window: {}",
                    present_result.get_report());
            }
        }

        // Clean up window state after rendering completes.
        for (const auto& window_id : windows_to_remove)
        {
            remove_window_state(window_id, false);
        }
    }

    void OpenGlRenderingPlugin::on_recieve_message(Message& msg)
    {
        if (auto* ready_event = handle_message<WindowContextReadyEvent>(msg))
        {
            handle_window_ready(*ready_event);
            return;
        }

        if (auto* open_event = handle_property_changed<&Window::is_open>(msg))
        {
            handle_window_open_changed(*open_event);
            return;
        }

        if (auto* size_event = handle_property_changed<&Window::size>(msg))
        {
            handle_window_resized(*size_event);
            return;
        }

        if (auto* resolution_event = handle_property_changed<&AppSettings::resolution>(msg))
        {
            handle_resolution_changed(*resolution_event);
            return;
        }
    }

    void OpenGlRenderingPlugin::handle_window_ready(WindowContextReadyEvent& event)
    {
        bool loaded = false;

        // TODO: Make an abstraction for GL loader selection
#ifdef USING_SDL3
        loaded = gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress));
#endif

        if (loaded)
        {
            initialize_opengl();
            _is_gl_ready = true;
        }
        else
            TBX_TRACE_WARNING("Failed to load glad!");

        if (!_is_gl_ready)
        {
            event.state = MessageState::ERROR;
            event.result.flag_failure("OpenGL rendering: GL loader not initialized.");
            return;
        }

        _window_sizes[event.window] = event.size;

        event.state = MessageState::HANDLED;
        event.result.flag_success();
    }

    void OpenGlRenderingPlugin::handle_window_open_changed(
        PropertyChangedEvent<Window, bool>& event)
    {
        if (!event.owner)
        {
            return;
        }

        if (event.current)
        {
            return;
        }

        remove_window_state(event.owner->id, true);
    }

    void OpenGlRenderingPlugin::handle_window_resized(PropertyChangedEvent<Window, Size>& event)
    {
        if (!event.owner)
        {
            return;
        }

        auto window_id = event.owner->id;
        if (_window_sizes.contains(window_id))
        {
            _window_sizes[window_id] = event.current;
        }
    }

    void OpenGlRenderingPlugin::handle_resolution_changed(
        PropertyChangedEvent<AppSettings, Size>& event)
    {
        _render_resolution = event.current;
    }

    void OpenGlRenderingPlugin::initialize_opengl() const
    {
        TBX_TRACE_INFO("OpenGL rendering: initializing OpenGL.");
        TBX_TRACE_INFO("OpenGL rendering: OpenGL info:");

        const auto* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const auto* glsl = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));

        TBX_TRACE_INFO("  Vendor: {}", vendor ? vendor : "Unknown");
        TBX_TRACE_INFO("  Renderer: {}", renderer ? renderer : "Unknown");
        TBX_TRACE_INFO("  Version: {}", version ? version : "Unknown");
        TBX_TRACE_INFO("  GLSL: {}", glsl ? glsl : "Unknown");

        TBX_ASSERT(
            GLVersion.major > 4 || (GLVersion.major == 4 && GLVersion.minor >= 5),
            "OpenGL rendering: requires OpenGL 4.5 or newer.");

#if defined(TBX_DEBUG)
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageControl(
            GL_DONT_CARE,
            GL_DONT_CARE,
            GL_DEBUG_SEVERITY_NOTIFICATION,
            0,
            nullptr,
            GL_FALSE);
        glDebugMessageCallback(gl_message_callback, nullptr);
#endif

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glDepthFunc(GL_LEQUAL);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    void OpenGlRenderingPlugin::initialize_present_pipeline(PresentPipeline& pipeline)
    {
        if (pipeline.program != 0U)
        {
            return;
        }

        static constexpr const char* VERTEX_SOURCE = R"GLSL(
#version 450 core
out vec2 v_uv;
void main()
{
    vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    vec2 uvs[3] = vec2[3](
        vec2(0.0, 0.0),
        vec2(2.0, 0.0),
        vec2(0.0, 2.0)
    );
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
    v_uv = uvs[gl_VertexID];
}
)GLSL";

        static constexpr const char* FRAGMENT_SOURCE = R"GLSL(
#version 450 core
in vec2 v_uv;
uniform sampler2D u_texture;
out vec4 out_color;
void main()
{
    out_color = texture(u_texture, v_uv);
}
)GLSL";

        const GLuint VERTEX_SHADER = compile_shader(GL_VERTEX_SHADER, VERTEX_SOURCE);
        if (VERTEX_SHADER == 0U)
        {
            return;
        }

        const GLuint FRAGMENT_SHADER = compile_shader(GL_FRAGMENT_SHADER, FRAGMENT_SOURCE);
        if (FRAGMENT_SHADER == 0U)
        {
            glDeleteShader(VERTEX_SHADER);
            return;
        }

        const GLuint PROGRAM = link_program(VERTEX_SHADER, FRAGMENT_SHADER);
        glDeleteShader(VERTEX_SHADER);
        glDeleteShader(FRAGMENT_SHADER);
        if (PROGRAM == 0U)
        {
            return;
        }

        GLuint vertex_array = 0U;
        glGenVertexArrays(1, &vertex_array);

        const GLint TEXTURE_UNIFORM = glGetUniformLocation(PROGRAM, "u_texture");
        if (TEXTURE_UNIFORM < 0)
        {
            TBX_TRACE_WARNING("OpenGL rendering: present pipeline uniform 'u_texture' not found.");
        }

        pipeline.program = static_cast<uint32>(PROGRAM);
        pipeline.vertex_array = static_cast<uint32>(vertex_array);
        pipeline.texture_uniform = TEXTURE_UNIFORM;
    }

    void OpenGlRenderingPlugin::destroy_present_pipeline(PresentPipeline& pipeline)
    {
        if (pipeline.vertex_array != 0U)
        {
            auto id = static_cast<GLuint>(pipeline.vertex_array);
            glDeleteVertexArrays(1, &id);
        }

        if (pipeline.program != 0U)
        {
            glDeleteProgram(static_cast<GLuint>(pipeline.program));
        }

        pipeline = {};
    }

    Size OpenGlRenderingPlugin::get_effective_resolution(const Size& window_size) const
    {
        Size resolution = _render_resolution;
        if (resolution.width == 0U || resolution.height == 0U)
        {
            resolution = window_size;
        }

        if (resolution.width == 0U)
        {
            resolution.width = 1U;
        }

        if (resolution.height == 0U)
        {
            resolution.height = 1U;
        }

        return resolution;
    }

    void OpenGlRenderingPlugin::remove_window_state(const Uuid& window_id, bool try_release)
    {
        _window_sizes.erase(window_id);

        auto it = _render_targets.find(window_id);
        if (it == _render_targets.end())
        {
            return;
        }

        if (try_release)
        {
            auto result = send_message<WindowMakeCurrentRequest>(window_id);
            if (result)
            {
                destroy_render_target(it->second);
                destroy_present_pipeline(it->second.present);
            }
        }

        _render_targets.erase(it);
    }

    void OpenGlRenderingPlugin::draw_models(const Mat4& view_projection)
    {
        auto& ecs = get_host().get_entity_registry();
        auto& asset_manager = get_host().get_asset_manager();

        static const auto FALLBACK_MATERIAL = std::make_shared<Material>();

        ecs.for_each_with<Renderer>(
            [&](Entity entity)
            {
                const Renderer& renderer = entity.get_component<Renderer>();
                if (!renderer.data)
                {
                    return;
                }

                // Step 2: Compute the entity model matrix (identity if no transform).
                Mat4 entity_matrix = Mat4(1.0f);
                if (entity.has_component<Transform>())
                {
                    entity_matrix = build_model_matrix(entity.get_component<Transform>());
                }

                if (auto* static_data = dynamic_cast<const StaticRenderData*>(renderer.data.get()))
                {
                    // Step 3: Draw meshes from a static model asset.
                    draw_static_model(
                        asset_manager,
                        *static_data,
                        entity_matrix,
                        view_projection,
                        FALLBACK_MATERIAL);
                    return;
                }

                if (auto* procedural_data =
                        dynamic_cast<const ProceduralData*>(renderer.data.get()))
                {
                    // Step 4: Draw procedurally supplied meshes.
                    draw_procedural_meshes(
                        asset_manager,
                        *procedural_data,
                        entity_matrix,
                        view_projection,
                        FALLBACK_MATERIAL);
                }
            });
    }

    OpenGlModel* OpenGlRenderingPlugin::get_cached_model(
        AssetManager& asset_manager,
        const Handle& handle)
    {
        if (!handle.is_valid())
        {
            return nullptr;
        }

        Uuid model_id = asset_manager.resolve_asset_id(handle);
        auto model_it = _models.find(model_id);
        if (model_it != _models.end())
        {
            return &model_it->second;
        }

        auto model_asset = asset_manager.load<Model>(handle);
        if (!model_asset)
        {
            return nullptr;
        }

        OpenGlModel gl_model = {};
        gl_model.model_id = model_id;
        gl_model.meshes.reserve(model_asset->meshes.size());
        gl_model.parts.reserve(model_asset->parts.size());

        for (size_t mesh_index = 0U; mesh_index < model_asset->meshes.size(); ++mesh_index)
        {
            const Mesh& mesh = model_asset->meshes[mesh_index];
            if (mesh.vertices.empty() || mesh.indices.empty())
            {
                gl_model.meshes.push_back({});
                continue;
            }

            Uuid mesh_key = make_model_mesh_key(model_id, static_cast<uint32>(mesh_index));
            get_mesh(mesh, mesh_key);
            gl_model.meshes.push_back(mesh_key);
        }

        for (const auto& part : model_asset->parts)
        {
            OpenGlModelPart gl_part = {};
            gl_part.transform = part.transform;
            gl_part.children = part.children;

            if (part.mesh_index < gl_model.meshes.size())
            {
                gl_part.mesh_id = gl_model.meshes[part.mesh_index];
            }

            if (part.material_index < model_asset->materials.size())
            {
                Uuid material_key =
                    Uuid::combine(model_id, static_cast<uint32>(part.material_index));
                gl_part.material_id = material_key;

                if (_materials.find(material_key) == _materials.end())
                {
                    OpenGlMaterial gl_material = {};
                    const Material& material = model_asset->materials[part.material_index];
                    if (try_build_gl_material(asset_manager, material, gl_material))
                    {
                        _materials.emplace(material_key, std::move(gl_material));
                    }
                    else
                    {
                        gl_part.material_id = {};
                    }
                }
            }

            gl_model.parts.push_back(std::move(gl_part));
        }

        auto [inserted, was_inserted] = _models.emplace(model_id, std::move(gl_model));
        static_cast<void>(was_inserted);
        return &inserted->second;
    }

    OpenGlMaterial* OpenGlRenderingPlugin::get_cached_material(
        AssetManager& asset_manager,
        const Handle& handle,
        const std::shared_ptr<Material>& fallback_material)
    {
        Handle resolved = handle.is_valid() ? handle : default_material;
        Uuid material_id = asset_manager.resolve_asset_id(resolved);
        auto material_it = _materials.find(material_id);
        if (material_it != _materials.end())
        {
            return &material_it->second;
        }

        auto material_asset = asset_manager.load<Material>(resolved);
        if (!material_asset)
        {
            return get_cached_fallback_material(asset_manager, fallback_material);
        }

        AssetUsage usage = asset_manager.get_usage<Material>(resolved);
        if (usage.stream_state != AssetStreamState::LOADED)
        {
            return resolved.id == default_material.id
                       ? get_cached_fallback_material(asset_manager, fallback_material)
                       : get_cached_material(asset_manager, default_material, fallback_material);
        }

        OpenGlMaterial gl_material = {};
        if (!try_build_gl_material(asset_manager, *material_asset, gl_material))
        {
            return get_cached_fallback_material(asset_manager, fallback_material);
        }

        auto [inserted, was_inserted] = _materials.emplace(material_id, std::move(gl_material));
        static_cast<void>(was_inserted);
        return &inserted->second;
    }

    OpenGlMaterial* OpenGlRenderingPlugin::get_cached_fallback_material(
        AssetManager& asset_manager,
        const std::shared_ptr<Material>& fallback_material)
    {
        if (_has_fallback_material)
        {
            return &_fallback_material;
        }

        Material fallback = Material();
        const Material& source = fallback_material ? *fallback_material : fallback;
        OpenGlMaterial gl_material = {};
        if (try_build_gl_material(asset_manager, source, gl_material))
        {
            _fallback_material = std::move(gl_material);
        }
        else
        {
            _fallback_material = {};
        }

        _has_fallback_material = true;
        return &_fallback_material;
    }

    bool OpenGlRenderingPlugin::try_build_gl_material(
        AssetManager& asset_manager,
        const Material& source_material,
        OpenGlMaterial& out_material)
    {
        OpenGlMaterial gl_material = {};
        gl_material.parameters.reserve(source_material.parameters.size());
        for (const auto& parameter : source_material.parameters)
        {
            ShaderUniform mapped = parameter;
            mapped.name = to_uniform_name(parameter.name);
            gl_material.parameters.push_back(std::move(mapped));
        }
        gl_material.textures.reserve(source_material.textures.size());

        std::vector<Handle> shader_handles = source_material.shaders;
        if (shader_handles.empty())
        {
            shader_handles.push_back(default_shader);
        }

        for (auto shader_handle : shader_handles)
        {
            auto resolved_handle = shader_handle;
            if (!ensure_shader_program(asset_manager, resolved_handle))
            {
                continue;
            }

            gl_material.shader_programs.push_back(asset_manager.resolve_asset_id(resolved_handle));
        }

        for (const auto& texture_binding : source_material.textures)
        {
            OpenGlMaterialTexture entry = {};
            entry.name = to_uniform_name(texture_binding.name);

            if (!texture_binding.handle.is_valid())
            {
                gl_material.textures.push_back(std::move(entry));
                continue;
            }

            auto texture_asset = asset_manager.load<Texture>(texture_binding.handle);
            if (!texture_asset)
            {
                gl_material.textures.push_back(std::move(entry));
                continue;
            }

            AssetUsage texture_usage = asset_manager.get_usage<Texture>(texture_binding.handle);
            if (texture_usage.stream_state != AssetStreamState::LOADED)
            {
                gl_material.textures.push_back(std::move(entry));
                continue;
            }

            Uuid texture_id = asset_manager.resolve_asset_id(texture_binding.handle);
            get_texture(texture_id, *texture_asset);
            entry.texture_id = texture_id;
            gl_material.textures.push_back(std::move(entry));
        }

        ensure_default_shader_textures(gl_material);

        if (gl_material.shader_programs.empty())
        {
            return false;
        }

        out_material = std::move(gl_material);
        return true;
    }

    std::shared_ptr<OpenGlShaderProgram> OpenGlRenderingPlugin::ensure_shader_program(
        AssetManager& asset_manager,
        Handle& shader_handle)
    {
        if (!shader_handle.is_valid())
        {
            shader_handle = default_shader;
        }

        Uuid shader_id = asset_manager.resolve_asset_id(shader_handle);
        auto program_it = _shader_programs.find(shader_id);
        if (program_it != _shader_programs.end())
        {
            return program_it->second;
        }

        auto shader_asset = asset_manager.load<Shader>(shader_handle);
        if (!shader_asset)
        {
            return {};
        }

        AssetUsage shader_usage = asset_manager.get_usage<Shader>(shader_handle);
        if (shader_usage.stream_state == AssetStreamState::LOADED)
        {
            return get_shader_program(asset_manager.resolve_asset_id(shader_handle), *shader_asset);
        }

        if (shader_handle.id == default_shader.id)
        {
            return {};
        }

        shader_handle = default_shader;
        Uuid fallback_id = asset_manager.resolve_asset_id(shader_handle);
        auto fallback_it = _shader_programs.find(fallback_id);
        if (fallback_it != _shader_programs.end())
        {
            return fallback_it->second;
        }

        shader_asset = asset_manager.load<Shader>(shader_handle);
        if (!shader_asset)
        {
            return {};
        }

        return get_shader_program(asset_manager.resolve_asset_id(shader_handle), *shader_asset);
    }

    std::shared_ptr<OpenGlMesh> OpenGlRenderingPlugin::get_cached_mesh(const Uuid& mesh_key) const
    {
        auto mesh_it = _meshes.find(mesh_key);
        if (mesh_it == _meshes.end())
        {
            return {};
        }

        return mesh_it->second;
    }

    void OpenGlRenderingPlugin::draw_mesh_with_material(
        const Uuid& mesh_key,
        const OpenGlMaterial& material,
        const Mat4& model_matrix,
        const Mat4& view_projection)
    {
        // Step 1: Fetch the mesh resource for this cache key.
        auto mesh_resource = get_cached_mesh(mesh_key);
        if (!mesh_resource)
            return;

        // Step 2: Determine which shader programs to render with.
        std::vector<Uuid> shader_programs = material.shader_programs;
        if (shader_programs.empty())
            shader_programs.push_back(default_shader.id);

        for (const auto& shader_id : shader_programs)
        {
            auto program_it = _shader_programs.find(shader_id);
            if (program_it == _shader_programs.end())
                continue;

            auto program = program_it->second;
            if (!program)
                continue;

            // Step 3: Bind the shader program and upload view/model matrices.
            GlResourceScope program_scope(*program);

            program->try_upload({
                .name = "u_view_proj",
                .data = view_projection,
            });
            program->try_upload({
                .name = "u_model",
                .data = model_matrix,
            });

            // Step 4: Upload scalar/vector material parameters.
            for (const auto& parameter : material.parameters)
                program->upload(parameter);

            // Step 5: Bind textures and upload sampler slots.
            std::vector<GlResourceScope> texture_scopes = {};
            texture_scopes.reserve(material.textures.size());
            uint32 texture_slot = 0U;

            for (const auto& texture_binding : material.textures)
            {
                auto texture_resource = get_default_texture();
                if (texture_binding.texture_id.is_valid())
                {
                    auto texture_it = _textures.find(texture_binding.texture_id);
                    if (texture_it != _textures.end() && texture_it->second)
                        texture_resource = texture_it->second;
                }

                if (texture_resource)
                {
                    texture_resource->set_slot(static_cast<int>(texture_slot));
                    texture_scopes.emplace_back(*texture_resource);
                }

                program->upload({
                    .name = texture_binding.name,
                    .data = static_cast<int>(texture_slot),
                });
                ++texture_slot;
            }

            // Step 6: Bind the mesh and issue the draw call.
            GlResourceScope mesh_scope(*mesh_resource);
            mesh_resource->draw();
        }
    }

    void OpenGlRenderingPlugin::draw_static_model(
        AssetManager& asset_manager,
        const StaticRenderData& static_data,
        const Mat4& entity_matrix,
        const Mat4& view_projection,
        const std::shared_ptr<Material>& fallback_material)
    {
        // Step 1: Validate and cache the model asset.
        auto model_asset = get_cached_model(asset_manager, static_data.model);
        if (!model_asset)
        {
            return;
        }

        // Step 2: Resolve the cached material for this model.
        auto fallback_gl_material = get_cached_fallback_material(asset_manager, fallback_material);
        if (!fallback_gl_material)
        {
            return;
        }

        OpenGlMaterial* override_material = nullptr;
        if (static_data.material.is_valid())
        {
            override_material =
                get_cached_material(asset_manager, static_data.material, fallback_material);
        }

        auto part_count = model_asset->parts.size();

        // Step 3: Draw meshes directly when the model has no part hierarchy.
        if (part_count == 0U)
        {
            const OpenGlMaterial* material =
                override_material ? override_material : fallback_gl_material;
            for (const auto& mesh_key : model_asset->meshes)
            {
                if (!mesh_key.is_valid())
                {
                    continue;
                }

                draw_mesh_with_material(mesh_key, *material, entity_matrix, view_projection);
            }
            return;
        }

        // Step 4: Traverse the part hierarchy so transforms are applied in order.
        draw_model_parts(
            *model_asset,
            entity_matrix,
            view_projection,
            override_material,
            *fallback_gl_material);
    }

    void OpenGlRenderingPlugin::draw_model_parts(
        const OpenGlModel& model,
        const Mat4& entity_matrix,
        const Mat4& view_projection,
        const OpenGlMaterial* override_material,
        const OpenGlMaterial& fallback_material)
    {
        auto part_count = model.parts.size();
        std::vector<bool> is_child = std::vector<bool>(part_count, false);

        // Step 4a: Mark parts that are referenced as children.
        for (const auto& part : model.parts)
        {
            for (const auto& child_index : part.children)
            {
                if (child_index < part_count)
                {
                    is_child[child_index] = true;
                }
            }
        }

        bool has_root = false;
        for (uint32 part_index = 0U; part_index < part_count; ++part_index)
        {
            if (!is_child[part_index])
            {
                has_root = true;
                draw_model_part_recursive(
                    model,
                    entity_matrix,
                    view_projection,
                    override_material,
                    fallback_material,
                    part_index);
            }
        }

        // Step 4b: If no roots were found, draw all parts from the entity transform.
        if (!has_root)
        {
            for (uint32 part_index = 0U; part_index < part_count; ++part_index)
            {
                draw_model_part_recursive(
                    model,
                    entity_matrix,
                    view_projection,
                    override_material,
                    fallback_material,
                    part_index);
            }
        }
    }

    void OpenGlRenderingPlugin::draw_model_part_recursive(
        const OpenGlModel& model,
        const Mat4& parent_matrix,
        const Mat4& view_projection,
        const OpenGlMaterial* override_material,
        const OpenGlMaterial& fallback_material,
        uint32 part_index)
    {
        if (part_index >= model.parts.size())
        {
            return;
        }

        const OpenGlModelPart& part = model.parts[part_index];

        // Step 4c: Combine the parent transform with this part's local transform.
        Mat4 part_matrix = parent_matrix * part.transform;

        // Step 4d: Draw the mesh bound to this part, if any.
        if (part.mesh_id.is_valid())
        {
            const OpenGlMaterial* material = override_material;
            if (!material)
            {
                auto material_it = _materials.find(part.material_id);
                material = material_it != _materials.end() ? &material_it->second : nullptr;
            }
            if (!material)
            {
                material = &fallback_material;
            }

            draw_mesh_with_material(part.mesh_id, *material, part_matrix, view_projection);
        }

        // Step 4e: Recurse into child parts.
        for (const auto& child_index : part.children)
        {
            draw_model_part_recursive(
                model,
                part_matrix,
                view_projection,
                override_material,
                fallback_material,
                child_index);
        }
    }

    void OpenGlRenderingPlugin::draw_procedural_meshes(
        AssetManager& asset_manager,
        const ProceduralData& procedural_data,
        const Mat4& entity_matrix,
        const Mat4& view_projection,
        const std::shared_ptr<Material>& fallback_material)
    {
        // Step 1: Iterate the procedural mesh list.
        const size_t mesh_count = procedural_data.meshes.size();
        for (size_t mesh_index = 0U; mesh_index < mesh_count; ++mesh_index)
        {
            const Mesh& mesh = procedural_data.meshes[mesh_index];
            if (mesh.vertices.empty() || mesh.indices.empty())
            {
                continue;
            }

            // Step 2: Resolve a material for this mesh index (or use default).
            Handle material_handle = {};
            if (mesh_index < procedural_data.materials.size())
            {
                material_handle = procedural_data.materials[mesh_index];
            }

            auto material_asset =
                get_cached_material(asset_manager, material_handle, fallback_material);
            if (!material_asset)
            {
                continue;
            }

            // Step 3: Draw the mesh with a stable cache key.
            Uuid mesh_key =
                make_procedural_mesh_key(procedural_data.id, static_cast<uint32>(mesh_index));
            auto mesh_resource = get_mesh(mesh, mesh_key);
            if (!mesh_resource)
            {
                continue;
            }
            draw_mesh_with_material(mesh_key, *material_asset, entity_matrix, view_projection);
        }
    }

    void OpenGlRenderingPlugin::draw_models_for_cameras(const Size& window_size)
    {
        auto& ecs = get_host().get_entity_registry();
        auto cameras = ecs.get_with<Camera, Transform>();

        if (cameras.begin() == cameras.end())
        {
            float aspect = window_size.get_aspect_ratio();
            Mat4 default_view =
                look_at(Vec3(0.0f, 0.0f, 5.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
            Mat4 default_projection =
                perspective_projection(degrees_to_radians(60.0f), aspect, 0.1f, 1000.0f);
            Mat4 view_projection = default_projection * default_view;
            draw_models(view_projection);
            return;
        }

        float aspect = window_size.get_aspect_ratio();
        for (const auto& camera_ent : cameras)
        {
            auto& camera = camera_ent.get_component<Camera>();
            camera.set_aspect(aspect);

            Transform& transform = camera_ent.get_component<Transform>();
            Mat4 view_projection =
                camera.get_view_projection_matrix(transform.position, transform.rotation);

            draw_models(view_projection);
        }
    }

    std::shared_ptr<OpenGlMesh> OpenGlRenderingPlugin::get_mesh(
        const Mesh& mesh,
        const Uuid& mesh_key)
    {
        if (auto it = _meshes.find(mesh_key); it != _meshes.end())
        {
            return it->second;
        }

        auto resource = std::make_shared<OpenGlMesh>(mesh);
        _meshes.emplace(mesh_key, resource);
        return resource;
    }

    static Uuid make_shader_source_key(const Uuid& shader_id, uint32 source_index)
    {
        Uuid resolved = shader_id.is_valid() ? shader_id : default_shader.id;
        return Uuid::combine(resolved, source_index);
    }

    std::shared_ptr<OpenGlShader> OpenGlRenderingPlugin::get_shader(
        const ShaderSource& shader,
        const Uuid& shader_key)
    {
        if (auto it = _shaders.find(shader_key); it != _shaders.end())
        {
            return it->second;
        }

        auto resource = std::make_shared<OpenGlShader>(shader);
        _shaders.emplace(shader_key, resource);
        return resource;
    }

    std::shared_ptr<OpenGlShaderProgram> OpenGlRenderingPlugin::get_shader_program(
        const Uuid& shader_id,
        const Shader& shader)
    {
        auto program_id = shader_id.is_valid() ? shader_id : default_shader.id;
        if (auto it = _shader_programs.find(program_id); it != _shader_programs.end())
        {
            return it->second;
        }

        std::vector<std::shared_ptr<OpenGlShader>> shaders = {};
        shaders.reserve(shader.sources.size());
        for (size_t source_index = 0U; source_index < shader.sources.size(); ++source_index)
        {
            const ShaderSource& shader_source = shader.sources[source_index];
            Uuid shader_key = make_shader_source_key(program_id, static_cast<uint32>(source_index));
            shaders.push_back(get_shader(shader_source, shader_key));
        }

        if (shaders.empty())
        {
            TBX_TRACE_WARNING("OpenGL rendering: shader program has no shaders.");
            return nullptr;
        }

        auto program = std::make_shared<OpenGlShaderProgram>(shaders);
        _shader_programs.emplace(program_id, program);
        return program;
    }

    std::shared_ptr<OpenGlTexture> OpenGlRenderingPlugin::get_default_texture()
    {
        if (_default_texture)
        {
            return _default_texture;
        }

        Texture default_texture = Texture(
            Size(1, 1),
            TextureWrap::REPEAT,
            TextureFilter::NEAREST,
            TextureFormat::RGBA,
            {255, 255, 255, 255});
        _default_texture = std::make_shared<OpenGlTexture>(default_texture);
        return _default_texture;
    }

    std::shared_ptr<OpenGlTexture> OpenGlRenderingPlugin::get_texture(
        const Uuid& texture_id,
        const Texture& texture)
    {
        Uuid texture_key = texture_id;
        if (auto it = _textures.find(texture_key); it != _textures.end())
        {
            return it->second;
        }

        auto resource = std::make_shared<OpenGlTexture>(texture);
        _textures.emplace(texture_key, resource);
        return resource;
    }
}
