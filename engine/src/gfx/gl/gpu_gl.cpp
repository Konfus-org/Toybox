#include "tbx/gfx/gpu.h"
#include "tbx/debug/log.h"
#include <glad/glad.h>
#include <cstddef>
#include <cstring>
#include <memory>
#include <numeric>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace tbx::gfx
{
    // Mirrors of current-context state owned by the process-global GL context —
    // deliberately not Runtime state (main thread only, no teardown).
    static int g_viewport_width = 0;
    static int g_viewport_height = 0;
    // The current pass's attachment height (UI scissor y-flip); mirrors of context
    // state owned by the process-global GL context, deliberately not Runtime state.
    static int g_drawable_height = 0;
    static Color g_clear_color = {};

    //// HELPERS ////

    static Result<GLuint> compile_stage(const GLenum stage, const std::string_view source)
    {
        const GLuint shader = glCreateShader(stage);
        const GLchar* text = source.data();
        const auto length = static_cast<GLint>(source.size());
        glShaderSource(shader, 1, &text, &length);
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

    Pipeline::~Pipeline() = default;

    RenderTarget::~RenderTarget()
    {
        glDeleteRenderbuffers(1, &_depth_buffer);
        glDeleteTextures(1, &_color_texture);
        glDeleteFramebuffers(1, &_framebuffer);
    }

    //// GPU ////

    static void clear_attachments(const Color& color)
    {
        g_clear_color = color;
        glClearColor(color.r, color.g, color.b, color.a);
        glDepthMask(GL_TRUE); // glClear respects the depth mask; a pass clear never should
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void begin_frame(const FrameDescription& description)
    {
        if (description.width > 0 && description.height > 0)
        {
            g_viewport_width = description.width;
            g_viewport_height = description.height;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, g_viewport_width, g_viewport_height);
        g_drawable_height = g_viewport_height;
        clear_attachments(description.clear);
    }

    void begin_render_pass(const RenderPassDescription& description)
    {
        if (description.depth_target)
        {
            // Depth-only passes always start empty; a shadow map never keeps stale depth.
            const DepthTarget& target = *description.depth_target;
            glBindFramebuffer(GL_FRAMEBUFFER, target.get_framebuffer());
            glViewport(0, 0, target.get_resolution(), target.get_resolution());
            g_drawable_height = target.get_resolution();
            glDepthMask(GL_TRUE);
            glClear(GL_DEPTH_BUFFER_BIT);
            return;
        }
        if (description.color_target)
        {
            const RenderTarget& target = *description.color_target;
            glBindFramebuffer(GL_FRAMEBUFFER, target.get_framebuffer());
            glViewport(0, 0, target.get_width(), target.get_height());
            g_drawable_height = target.get_height();
        }
        else
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, g_viewport_width, g_viewport_height);
            g_drawable_height = g_viewport_height;
        }
        if (description.load == LoadOperation::CLEAR)
            clear_attachments(description.clear_color);
    }

    void end_render_pass()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, g_viewport_width, g_viewport_height);
        g_drawable_height = g_viewport_height;
    }

    Color get_clear_color()
    {
        return g_clear_color;
    }

    Result<std::unique_ptr<Shader>> compile_shader(
        const std::string_view vertex_source,
        const std::string_view fragment_source)
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

    std::unique_ptr<Pipeline> make_pipeline(const PipelineDescription& description)
    {
        // GL has no baked pipeline objects; the description IS the object and set_pipeline
        // applies it as state.
        return std::make_unique<Pipeline>(description);
    }

    void set_pipeline(const Pipeline& pipeline)
    {
        const PipelineDescription& description = pipeline.get_description();
        glUseProgram(description.shader.get().get_id());
        if (description.is_depth_test_enabled)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
        glDepthMask(description.is_depth_write_enabled ? GL_TRUE : GL_FALSE);
        switch (description.cull)
        {
            case CullMode::NONE:
                glDisable(GL_CULL_FACE);
                break;
            case CullMode::BACK:
                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);
                break;
            case CullMode::FRONT:
                glEnable(GL_CULL_FACE);
                glCullFace(GL_FRONT);
                break;
        }
        switch (description.blend)
        {
            case BlendMode::NONE:
                glDisable(GL_BLEND);
                break;
            case BlendMode::ALPHA:
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                break;
            case BlendMode::PREMULTIPLIED:
                glEnable(GL_BLEND);
                glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                break;
        }
    }

    void draw(const Mesh& mesh, const std::span<const TextureBinding> textures)
    {
        for (const TextureBinding& binding : textures)
        {
            glActiveTexture(GL_TEXTURE0 + binding.slot);
            std::visit(
                [](const auto& texture)
                {
                    using TTexture = std::decay_t<decltype(texture.get())>;
                    if constexpr (std::is_same_v<TTexture, Texture2d>)
                        glBindTexture(GL_TEXTURE_2D, texture.get().get_id());
                    else if constexpr (std::is_same_v<TTexture, DepthTarget>)
                        glBindTexture(GL_TEXTURE_2D, texture.get().get_depth_texture());
                    else
                        glBindTexture(GL_TEXTURE_2D, texture.get().get_color_texture());
                },
                binding.texture);
        }
        glBindVertexArray(mesh.get_vertex_array());
        glDrawArrays(GL_TRIANGLES, 0, mesh.get_vertex_count());
        glBindVertexArray(0);
    }

    void set_scissor(const bool is_enabled, const int x, const int y, const int width, const int height)
    {
        if (!is_enabled)
        {
            glDisable(GL_SCISSOR_TEST);
            return;
        }
        glEnable(GL_SCISSOR_TEST);
        // UI speaks y-down; GL scissor is y-up from the bottom of the current drawable.
        glScissor(x, g_drawable_height - (y + height), width, height);
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
            TBX_ERROR("failed to load OpenGL functions");
            std::abort();
        }
        TBX_INFO("OpenGL {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
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

    Result<void> screenshot(Texture& result)
    {
        const int width = g_viewport_width;
        const int height = g_viewport_height;
        if (width <= 0 || height <= 0)
            return fail("screenshot: no drawable is current");
        const size row_bytes = static_cast<size>(width) * 4;
        auto bottom_up = std::vector<std::byte>(row_bytes * height);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, bottom_up.data());

        // GL reads rows bottom-up; images are top-down.
        result.width = width;
        result.height = height;
        result.pixels.resize(bottom_up.size());
        for (int row = 0; row < height; ++row)
            std::memcpy(
                result.pixels.data() + static_cast<size>(row) * row_bytes,
                bottom_up.data() + static_cast<size>(height - 1 - row) * row_bytes,
                row_bytes);
        return {};
    }

    void set_viewport(const int x, const int y, const int width, const int height)
    {
        // A per-camera sub-rect inside the current pass; the drawable mirror stays put so
        // pass boundaries reset to the full window.
        glViewport(x, y, width, height);
    }

    void set_viewport(const int width, const int height)
    {
        g_viewport_width = width;
        g_viewport_height = height;
        g_drawable_height = height;
        glViewport(0, 0, width, height);
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
