#include "opengl_render_target.h"
#include <glad/glad.h>
#include <string>
#include <vector>


namespace tbx::plugins
{
    static std::shared_ptr<OpenGlShaderProgram> create_present_program()
    {
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

        auto vertex_shader = std::make_shared<OpenGlShader>(
            ShaderSource(std::string(VERTEX_SOURCE), ShaderType::VERTEX));
        if (vertex_shader->get_shader_id() == 0U)
        {
            return {};
        }

        auto fragment_shader = std::make_shared<OpenGlShader>(
            ShaderSource(std::string(FRAGMENT_SOURCE), ShaderType::FRAGMENT));
        if (fragment_shader->get_shader_id() == 0U)
        {
            return {};
        }

        std::vector<std::shared_ptr<OpenGlShader>> shaders = {};
        shaders.reserve(2U);
        shaders.push_back(std::move(vertex_shader));
        shaders.push_back(std::move(fragment_shader));

        return std::make_shared<OpenGlShaderProgram>(shaders);
    }

    bool OpenGlPresentPipeline::is_ready() const
    {
        if (!program)
        {
            return false;
        }

        return program->get_program_id() != 0U && vertex_array != 0U;
    }

    bool OpenGlPresentPipeline::try_initialize()
    {
        if (is_ready())
        {
            return true;
        }

        program = create_present_program();
        if (!program || program->get_program_id() == 0U)
        {
            program.reset();
            return false;
        }

        GLuint vertex_array = 0U;
        glGenVertexArrays(1, &vertex_array);
        if (vertex_array == 0U)
        {
            program.reset();
            return false;
        }

        this->vertex_array = static_cast<uint32>(vertex_array);
        return true;
    }

    void OpenGlPresentPipeline::destroy()
    {
        if (vertex_array != 0U)
        {
            auto id = static_cast<GLuint>(vertex_array);
            glDeleteVertexArrays(1, &id);
        }

        vertex_array = 0U;
        program.reset();
    }

    bool OpenGlRenderTarget::try_resize(const Size& size)
    {
        if (size.width == 0U || size.height == 0U)
        {
            destroy();
            return false;
        }

        if (_framebuffer != 0U && _size.width == size.width && _size.height == size.height)
        {
            return true;
        }

        if (_framebuffer == 0U)
        {
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

            _framebuffer = static_cast<uint32>(framebuffer);
            _color_texture = static_cast<uint32>(color_texture);
            _depth_stencil = static_cast<uint32>(depth_stencil);
        }
        else
        {
            glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(_framebuffer));

            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(_color_texture));
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
                static_cast<GLuint>(_color_texture),
                0);

            glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(_depth_stencil));
            glRenderbufferStorage(
                GL_RENDERBUFFER,
                GL_DEPTH24_STENCIL8,
                static_cast<GLsizei>(size.width),
                static_cast<GLsizei>(size.height));
            glFramebufferRenderbuffer(
                GL_FRAMEBUFFER,
                GL_DEPTH_STENCIL_ATTACHMENT,
                GL_RENDERBUFFER,
                static_cast<GLuint>(_depth_stencil));
        }

        auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            destroy();
            return false;
        }

        _size = size;
        return true;
    }

    void OpenGlRenderTarget::destroy()
    {
        if (_depth_stencil != 0U)
        {
            auto id = static_cast<GLuint>(_depth_stencil);
            glDeleteRenderbuffers(1, &id);
            _depth_stencil = 0U;
        }

        if (_color_texture != 0U)
        {
            auto id = static_cast<GLuint>(_color_texture);
            glDeleteTextures(1, &id);
            _color_texture = 0U;
        }

        if (_framebuffer != 0U)
        {
            auto id = static_cast<GLuint>(_framebuffer);
            glDeleteFramebuffers(1, &id);
            _framebuffer = 0U;
        }

        _size = {0U, 0U};
    }

    void OpenGlRenderTarget::destroy_present_pipeline()
    {
        _present_pipeline.destroy();
    }

    uint32 OpenGlRenderTarget::get_framebuffer() const
    {
        return _framebuffer;
    }

    uint32 OpenGlRenderTarget::get_color_texture() const
    {
        return _color_texture;
    }

    Size OpenGlRenderTarget::get_size() const
    {
        return _size;
    }

    OpenGlPresentPipeline& OpenGlRenderTarget::get_present_pipeline()
    {
        return _present_pipeline;
    }

    const OpenGlPresentPipeline& OpenGlRenderTarget::get_present_pipeline() const
    {
        return _present_pipeline;
    }
}
