#include "tbx/gfx/gpu.h"
#include "tbx/core/log.h"
#include <glad/glad.h>
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

    //// GPU ////

    static int g_viewport_width = 0;
    static int g_viewport_height = 0;

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
        glClearColor(color.r, color.g, color.b, color.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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

    void set_uniform(const Shader& shader, const char* name, const Mat4& value)
    {
        glUseProgram(shader.get_id());
        glUniformMatrix4fv(
            glGetUniformLocation(shader.get_id(), name), 1, GL_FALSE, &value[0][0]);
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
