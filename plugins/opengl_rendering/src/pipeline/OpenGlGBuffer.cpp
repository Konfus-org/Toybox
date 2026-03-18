#include "OpenGlGBuffer.h"
#include "tbx/debugging/macros.h"
#include <array>

namespace opengl_rendering
{
    static constexpr auto GBUFFER_DRAW_ATTACHMENTS = std::array<GLenum, 5U> {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
        GL_COLOR_ATTACHMENT3,
        GL_COLOR_ATTACHMENT4,
    };

    static GLenum get_stage_attachment(const tbx::RenderStage render_stage)
    {
        switch (render_stage)
        {
            case tbx::RenderStage::FINAL_COLOR:
                return GL_COLOR_ATTACHMENT0;
            case tbx::RenderStage::GEOMETRY_COLOR:
                return GL_COLOR_ATTACHMENT1;
            case tbx::RenderStage::GBUFFER_ALBEDO:
                return GL_COLOR_ATTACHMENT2;
            case tbx::RenderStage::GBUFFER_NORMAL:
                return GL_COLOR_ATTACHMENT3;
            case tbx::RenderStage::GBUFFER_DEPTH:
                return GL_COLOR_ATTACHMENT4;
            default:
                return GL_COLOR_ATTACHMENT0;
        }
    }

    OpenGlGBuffer::~OpenGlGBuffer() noexcept
    {
        destroy();
    }

    void OpenGlGBuffer::resize(const tbx::Size& size)
    {
        if (size.width == 0U || size.height == 0U)
            return;

        if (_size.width == size.width && _size.height == size.height && _framebuffer != 0U)
            return;

        destroy();
        _size = size;

        glGenFramebuffers(1, &_framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);

        _final_color = create_color_attachment(GL_RGBA16F, _size.width, _size.height);
        _geometry_color = create_color_attachment(GL_RGBA16F, _size.width, _size.height);
        _albedo = create_color_attachment(GL_RGBA8, _size.width, _size.height);
        _normal = create_color_attachment(GL_RGBA16F, _size.width, _size.height);
        _depth_visual = create_color_attachment(GL_RGBA16F, _size.width, _size.height);
        _depth = create_depth_attachment(_size.width, _size.height);

        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            _final_color,
            0);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT1,
            GL_TEXTURE_2D,
            _geometry_color,
            0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, _albedo, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, _normal, 0);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT4,
            GL_TEXTURE_2D,
            _depth_visual,
            0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _depth, 0);
        glDrawBuffers(
            static_cast<GLsizei>(GBUFFER_DRAW_ATTACHMENTS.size()),
            GBUFFER_DRAW_ATTACHMENTS.data());

        const auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        TBX_ASSERT(status == GL_FRAMEBUFFER_COMPLETE, "OpenGL g-buffer framebuffer is incomplete.");
        unbind();
    }

    void OpenGlGBuffer::bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
        glDrawBuffers(
            static_cast<GLsizei>(GBUFFER_DRAW_ATTACHMENTS.size()),
            GBUFFER_DRAW_ATTACHMENTS.data());
    }

    void OpenGlGBuffer::unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGlGBuffer::present(const tbx::RenderStage render_stage, const tbx::Size& viewport_size)
        const
    {
        if (_framebuffer == 0U || viewport_size.width == 0U || viewport_size.height == 0U)
            return;

        glBindFramebuffer(GL_READ_FRAMEBUFFER, _framebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glReadBuffer(get_stage_attachment(render_stage));
        glBlitFramebuffer(
            0,
            0,
            static_cast<GLint>(_size.width),
            static_cast<GLint>(_size.height),
            0,
            0,
            static_cast<GLint>(viewport_size.width),
            static_cast<GLint>(viewport_size.height),
            GL_COLOR_BUFFER_BIT,
            GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    GLuint OpenGlGBuffer::create_color_attachment(
        const GLenum internal_format,
        const tbx::uint32 width,
        const tbx::uint32 height)
    {
        auto texture_id = GLuint {0U};
        glGenTextures(1, &texture_id);
        glBindTexture(GL_TEXTURE_2D, texture_id);

        auto upload_type = GL_FLOAT;
        if (internal_format == GL_RGBA8)
            upload_type = GL_UNSIGNED_BYTE;

        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            static_cast<GLint>(internal_format),
            static_cast<GLsizei>(width),
            static_cast<GLsizei>(height),
            0,
            GL_RGBA,
            upload_type,
            nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        return texture_id;
    }

    GLuint OpenGlGBuffer::create_depth_attachment(const tbx::uint32 width, const tbx::uint32 height)
    {
        auto texture_id = GLuint {0U};
        glGenTextures(1, &texture_id);
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_DEPTH_COMPONENT24,
            static_cast<GLsizei>(width),
            static_cast<GLsizei>(height),
            0,
            GL_DEPTH_COMPONENT,
            GL_FLOAT,
            nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        return texture_id;
    }

    void OpenGlGBuffer::delete_texture(GLuint& texture_id)
    {
        if (texture_id == 0U)
            return;

        glDeleteTextures(1, &texture_id);
        texture_id = 0U;
    }

    void OpenGlGBuffer::destroy()
    {
        delete_texture(_final_color);
        delete_texture(_geometry_color);
        delete_texture(_albedo);
        delete_texture(_normal);
        delete_texture(_depth_visual);
        delete_texture(_depth);

        if (_framebuffer != 0U)
        {
            glDeleteFramebuffers(1, &_framebuffer);
            _framebuffer = 0U;
        }

        _size = {0U, 0U};
    }
}
