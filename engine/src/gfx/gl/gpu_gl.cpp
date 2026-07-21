#include "tbx/gfx/gpu.h"
#include "tbx/core/log.h"
#include <glad/glad.h>
#include <cstddef>
#include <memory>
#include <numeric>

namespace tbx::gpu
{
    //// HELPERS ////

    static Result<GLuint> compile_stage(const GLenum stage, const char* source)
    {
        const GLuint shader = glCreateShader(stage);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        GLint ok = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char info[1024] = {};
            glGetShaderInfoLog(shader, sizeof(info), nullptr, info);
            glDeleteShader(shader);
            return fail("shader compile failed: {}", info);
        }
        return shader;
    }

    //// RAII RELEASES ////

    Shader::~Shader()
    {
        glDeleteProgram(_id);
    }

    Mesh::~Mesh()
    {
        glDeleteBuffers(1, &_vertex_buffer);
        glDeleteVertexArrays(1, &_vertex_array);
    }

    Texture2d::~Texture2d()
    {
        glDeleteTextures(1, &_id);
    }

    DepthTarget::~DepthTarget()
    {
        glDeleteTextures(1, &_depth_texture);
        glDeleteFramebuffers(1, &_framebuffer);
    }

    RenderTarget::~RenderTarget()
    {
        glDeleteRenderbuffers(1, &_depth_buffer);
        glDeleteTextures(1, &_color_texture);
        glDeleteFramebuffers(1, &_framebuffer);
    }

    //// GPU ////

    static int g_viewport_width = 0;
    static int g_viewport_height = 0;
    static Color g_clear_color = {};

    void begin_frame(const FrameDescription& description)
    {
        if (description.width > 0 && description.height > 0)
        {
            g_viewport_width = description.width;
            g_viewport_height = description.height;
        }
        glViewport(0, 0, g_viewport_width, g_viewport_height);
        clear(description.clear);
    }

    void clear(const Color& color)
    {
        g_clear_color = color;
        glClearColor(color.r, color.g, color.b, color.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    Color get_clear_color()
    {
        return g_clear_color;
    }

    Result<std::unique_ptr<Shader>> compile_shader(
        const char* vertex_source,
        const char* fragment_source)
    {
        const auto vertex = compile_stage(GL_VERTEX_SHADER, vertex_source);
        if (!vertex)
            return std::unexpected(vertex.error());
        const auto fragment = compile_stage(GL_FRAGMENT_SHADER, fragment_source);
        if (!fragment)
        {
            glDeleteShader(*vertex);
            return std::unexpected(fragment.error());
        }

        const GLuint program = glCreateProgram();
        glAttachShader(program, *vertex);
        glAttachShader(program, *fragment);
        glLinkProgram(program);
        glDeleteShader(*vertex);
        glDeleteShader(*fragment);

        GLint ok = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok)
        {
            char info[1024] = {};
            glGetProgramInfoLog(program, sizeof(info), nullptr, info);
            glDeleteProgram(program);
            return fail("shader link failed: {}", info);
        }
        return std::make_unique<Shader>(program);
    }

    void draw(const Shader& shader, const Mesh& mesh)
    {
        glUseProgram(shader.get_id());
        glBindVertexArray(mesh.get_vertex_array());
        glDrawArrays(GL_TRIANGLES, 0, mesh.get_vertex_count());
        glBindVertexArray(0);
    }

    /// @brief
    /// Purpose: Lazily-built GL objects for the 2D UI path.
    struct UiPipeline
    {
        GLuint program = 0;
        GLuint vertex_array = 0;
        GLuint vertex_buffer = 0;
        GLuint index_buffer = 0;
        GLuint white_texture = 0;
    };

    static UiPipeline g_ui = {};

    static constexpr const char* UI_VERTEX_SHADER = R"(#version 460 core
layout(location = 0) in vec2 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_uv;
uniform vec2 in_screen;
uniform vec2 in_translation;
out vec4 v_color;
out vec2 v_uv;
void main()
{
    vec2 at = in_position + in_translation;
    v_color = in_color;
    v_uv = in_uv;
    gl_Position = vec4(at.x / in_screen.x * 2.0 - 1.0, 1.0 - at.y / in_screen.y * 2.0, 0.0, 1.0);
})";

    static constexpr const char* UI_FRAGMENT_SHADER = R"(#version 460 core
in vec4 v_color;
in vec2 v_uv;
uniform sampler2D in_texture;
out vec4 out_color;
void main()
{
    out_color = texture(in_texture, v_uv) * v_color;
})";

    static bool ensure_ui_pipeline()
    {
        if (g_ui.program)
            return true;
        auto shader = compile_shader(UI_VERTEX_SHADER, UI_FRAGMENT_SHADER);
        if (!shader)
        {
            log_error("ui shader failed: {}", shader.error());
            return false;
        }
        // The Shader RAII object owns the program; parked in a static for process lifetime.
        static std::unique_ptr<Shader> g_ui_shader = {};
        g_ui_shader = std::move(*shader);
        g_ui.program = g_ui_shader->get_id();
        glGenVertexArrays(1, &g_ui.vertex_array);
        glGenBuffers(1, &g_ui.vertex_buffer);
        glGenBuffers(1, &g_ui.index_buffer);
        glBindVertexArray(g_ui.vertex_array);
        glBindBuffer(GL_ARRAY_BUFFER, g_ui.vertex_buffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ui.index_buffer);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0, 2, GL_FLOAT, GL_FALSE, sizeof(UiVertex),
            reinterpret_cast<const void*>(offsetof(UiVertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(UiVertex),
            reinterpret_cast<const void*>(offsetof(UiVertex, color)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(
            2, 2, GL_FLOAT, GL_FALSE, sizeof(UiVertex),
            reinterpret_cast<const void*>(offsetof(UiVertex, uv)));
        glBindVertexArray(0);
        glGenTextures(1, &g_ui.white_texture);
        glBindTexture(GL_TEXTURE_2D, g_ui.white_texture);
        constexpr unsigned char WHITE[4] = {255, 255, 255, 255};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, WHITE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        return true;
    }

    void draw_ui(
        std::span<const UiVertex> vertices,
        std::span<const int> indices,
        std::optional<std::reference_wrapper<const Texture2d>> texture,
        const Vec2& translation)
    {
        if (!ensure_ui_pipeline())
            return;
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // RmlUi colors are premultiplied
        glDisable(GL_CULL_FACE);

        glUseProgram(g_ui.program);
        glUniform2f(
            glGetUniformLocation(g_ui.program, "in_screen"),
            static_cast<float>(g_viewport_width),
            static_cast<float>(g_viewport_height));
        glUniform2f(
            glGetUniformLocation(g_ui.program, "in_translation"), translation.x, translation.y);
        glUniform1i(glGetUniformLocation(g_ui.program, "in_texture"), 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture ? texture->get().get_id() : g_ui.white_texture);

        glBindVertexArray(g_ui.vertex_array);
        glBindBuffer(GL_ARRAY_BUFFER, g_ui.vertex_buffer);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(vertices.size_bytes()),
            vertices.data(),
            GL_STREAM_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ui.index_buffer);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(indices.size_bytes()),
            indices.data(),
            GL_STREAM_DRAW);
        glDrawElements(
            GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }

    void set_scissor(const bool is_enabled, const int x, const int y, const int width, const int height)
    {
        if (!is_enabled)
        {
            glDisable(GL_SCISSOR_TEST);
            return;
        }
        glEnable(GL_SCISSOR_TEST);
        // UI speaks y-down; GL scissor is y-up from the bottom.
        glScissor(x, g_viewport_height - (y + height), width, height);
    }

    int get_viewport_height()
    {
        return g_viewport_height;
    }

    int get_viewport_width()
    {
        return g_viewport_width;
    }

    void initialize()
    {
        // glad's own platform loader (wgl + opengl32) — no window/loader coupling here.
        if (!gladLoadGL())
        {
            log_error("failed to load OpenGL functions");
            std::abort();
        }
        log_info("OpenGL {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
    }

    ShaderInfo reflect(const Shader& shader)
    {
        auto info = ShaderInfo {};
        GLint count = 0;
        glGetProgramiv(shader.get_id(), GL_ACTIVE_UNIFORMS, &count);
        for (GLint i = 0; i < count; ++i)
        {
            char name[256] = {};
            GLsizei length = 0;
            GLint array_size = 0;
            GLenum gl_type = 0;
            glGetActiveUniform(
                shader.get_id(), static_cast<GLuint>(i), sizeof(name), &length, &array_size,
                &gl_type, name);
            auto kind = UniformKind::UNKNOWN;
            switch (gl_type)
            {
                case GL_FLOAT: kind = UniformKind::FLOAT; break;
                case GL_INT: kind = UniformKind::INT; break;
                case GL_BOOL: kind = UniformKind::BOOL; break;
                case GL_FLOAT_VEC2: kind = UniformKind::VEC2; break;
                case GL_FLOAT_VEC3: kind = UniformKind::VEC3; break;
                case GL_FLOAT_VEC4: kind = UniformKind::VEC4; break;
                case GL_FLOAT_MAT4: kind = UniformKind::MAT4; break;
                case GL_SAMPLER_2D: kind = UniformKind::TEXTURE; break;
                default: break;
            }
            info.uniforms.push_back({.name = std::string(name, length), .kind = kind});
        }
        return info;
    }

    Color read_pixel(const int x, const int y)
    {
        float rgba[4] = {};
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_FLOAT, rgba);
        return {.r = rgba[0], .g = rgba[1], .b = rgba[2], .a = rgba[3]};
    }

    void set_viewport(const int width, const int height)
    {
        g_viewport_width = width;
        g_viewport_height = height;
        glViewport(0, 0, width, height);
    }

    void begin_depth_pass(const DepthTarget& target)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, target.get_framebuffer());
        glViewport(0, 0, target.get_resolution(), target.get_resolution());
        glClear(GL_DEPTH_BUFFER_BIT);
        glCullFace(GL_FRONT); // reduces shadow acne on closed meshes
    }

    void bind_depth_texture(const DepthTarget& target, const int slot)
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, target.get_depth_texture());
    }

    void bind_texture(const Texture2d& texture, const int slot)
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, texture.get_id());
    }

    void end_depth_pass()
    {
        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, g_viewport_width, g_viewport_height);
    }

    std::unique_ptr<DepthTarget> make_depth_target(const int resolution)
    {
        GLuint depth_texture = 0;
        glGenTextures(1, &depth_texture);
        glBindTexture(GL_TEXTURE_2D, depth_texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_DEPTH_COMPONENT24,
            resolution,
            resolution,
            0,
            GL_DEPTH_COMPONENT,
            GL_FLOAT,
            nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        constexpr float BORDER[4] = {1.0f, 1.0f, 1.0f, 1.0f}; // outside the map = fully lit
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, BORDER);

        GLuint framebuffer = 0;
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return std::make_unique<DepthTarget>(framebuffer, depth_texture, resolution);
    }

    void begin_render_target(const RenderTarget& target)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, target.get_framebuffer());
        glViewport(0, 0, target.get_width(), target.get_height());
    }

    void bind_render_target_texture(const RenderTarget& target, const int slot)
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, target.get_color_texture());
    }

    void end_render_target()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, g_viewport_width, g_viewport_height);
    }

    std::unique_ptr<RenderTarget> make_render_target(const int width, const int height)
    {
        GLuint color_texture = 0;
        glGenTextures(1, &color_texture);
        glBindTexture(GL_TEXTURE_2D, color_texture);
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        GLuint depth_buffer = 0;
        glGenRenderbuffers(1, &depth_buffer);
        glBindRenderbuffer(GL_RENDERBUFFER, depth_buffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

        GLuint framebuffer = 0;
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_texture, 0);
        glFramebufferRenderbuffer(
            GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_buffer);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return std::make_unique<RenderTarget>(
            framebuffer, color_texture, depth_buffer, width, height);
    }

    void set_depth_test(const bool is_enabled)
    {
        if (is_enabled)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
    }

    void set_depth_write(const bool is_enabled)
    {
        glDepthMask(is_enabled ? GL_TRUE : GL_FALSE);
    }

    void set_uniform(const Shader& shader, const char* name, const Mat4& value)
    {
        glUseProgram(shader.get_id());
        glUniformMatrix4fv(
            glGetUniformLocation(shader.get_id(), name), 1, GL_FALSE, &value[0][0]);
    }

    void set_uniform(const Shader& shader, const char* name, const Vec2& value)
    {
        glUseProgram(shader.get_id());
        glUniform2f(glGetUniformLocation(shader.get_id(), name), value.x, value.y);
    }

    void set_uniform(const Shader& shader, const char* name, const Vec4& value)
    {
        glUseProgram(shader.get_id());
        glUniform4f(
            glGetUniformLocation(shader.get_id(), name), value.x, value.y, value.z, value.w);
    }

    void set_uniform(const Shader& shader, const char* name, const Vec3& value)
    {
        glUseProgram(shader.get_id());
        glUniform3f(glGetUniformLocation(shader.get_id(), name), value.x, value.y, value.z);
    }

    void set_uniform(const Shader& shader, const char* name, const Color& value)
    {
        glUseProgram(shader.get_id());
        glUniform4f(
            glGetUniformLocation(shader.get_id(), name), value.r, value.g, value.b, value.a);
    }

    void set_uniform(const Shader& shader, const char* name, const float value)
    {
        glUseProgram(shader.get_id());
        glUniform1f(glGetUniformLocation(shader.get_id(), name), value);
    }

    void set_uniform(const Shader& shader, const char* name, const int value)
    {
        glUseProgram(shader.get_id());
        glUniform1i(glGetUniformLocation(shader.get_id(), name), value);
    }

    std::unique_ptr<Texture2d> upload_texture(
        const int width,
        const int height,
        std::span<const std::byte> rgba_pixels)
    {
        GLuint id = 0;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            width,
            height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            rgba_pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        return std::make_unique<Texture2d>(id);
    }

    std::unique_ptr<Mesh> upload_mesh(
        std::span<const float> vertices,
        std::span<const int> attribute_sizes)
    {
        const int floats_per_vertex =
            std::accumulate(attribute_sizes.begin(), attribute_sizes.end(), 0);
        const int vertex_count = static_cast<int>(vertices.size()) / floats_per_vertex;

        GLuint vertex_array = 0;
        GLuint vertex_buffer = 0;
        glGenVertexArrays(1, &vertex_array);
        glGenBuffers(1, &vertex_buffer);
        glBindVertexArray(vertex_array);
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(vertices.size_bytes()),
            vertices.data(),
            GL_STATIC_DRAW);

        const auto stride = static_cast<GLsizei>(floats_per_vertex * sizeof(float));
        size offset = 0;
        for (size i = 0; i < attribute_sizes.size(); ++i)
        {
            glEnableVertexAttribArray(static_cast<GLuint>(i));
            glVertexAttribPointer(
                static_cast<GLuint>(i),
                attribute_sizes[i],
                GL_FLOAT,
                GL_FALSE,
                stride,
                reinterpret_cast<const void*>(offset * sizeof(float)));
            offset += static_cast<size>(attribute_sizes[i]);
        }
        glBindVertexArray(0);
        return std::make_unique<Mesh>(vertex_array, vertex_buffer, vertex_count);
    }
}
