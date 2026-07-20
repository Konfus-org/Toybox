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
