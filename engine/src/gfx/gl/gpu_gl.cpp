#include "tbx/gfx/gpu.h"
#include "tbx/core/log.h"
#include <glad/glad.h>
#include <numeric>

namespace tbx::gpu
{
    //// HELPERS ////

    static Result<GLuint> compile_stage(GLenum stage, const char* source)
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

    //// GPU ////

    void clear(const Color& color)
    {
        glClearColor(color.r, color.g, color.b, color.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    Mesh create_mesh(std::span<const float> vertices, std::span<const int> attribute_sizes)
    {
        auto mesh = Mesh {};
        const int floats_per_vertex =
            std::accumulate(attribute_sizes.begin(), attribute_sizes.end(), 0);
        mesh.vertex_count = static_cast<int>(vertices.size()) / floats_per_vertex;

        glGenVertexArrays(1, &mesh.vertex_array);
        glGenBuffers(1, &mesh.vertex_buffer);
        glBindVertexArray(mesh.vertex_array);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vertex_buffer);
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
        return mesh;
    }

    Result<Shader> create_shader(const char* vertex_source, const char* fragment_source)
    {
        auto vertex = compile_stage(GL_VERTEX_SHADER, vertex_source);
        if (!vertex)
            return std::unexpected(vertex.error());
        auto fragment = compile_stage(GL_FRAGMENT_SHADER, fragment_source);
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
        return Shader {.id = program};
    }

    void destroy_mesh(Mesh mesh)
    {
        glDeleteBuffers(1, &mesh.vertex_buffer);
        glDeleteVertexArrays(1, &mesh.vertex_array);
    }

    void destroy_shader(Shader shader)
    {
        glDeleteProgram(shader.id);
    }

    void draw(Shader shader, const Mesh& mesh)
    {
        glUseProgram(shader.id);
        glBindVertexArray(mesh.vertex_array);
        glDrawArrays(GL_TRIANGLES, 0, mesh.vertex_count);
        glBindVertexArray(0);
    }

    void init(Window::GlProcLoader loader)
    {
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(loader)))
        {
            log_error("failed to load OpenGL functions");
            std::abort();
        }
        log_info("OpenGL {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
        glEnable(GL_DEPTH_TEST);
    }

    Color read_pixel(int x, int y)
    {
        float rgba[4] = {};
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_FLOAT, rgba);
        return {.r = rgba[0], .g = rgba[1], .b = rgba[2], .a = rgba[3]};
    }

    void set_viewport(int width, int height)
    {
        glViewport(0, 0, width, height);
    }
}
