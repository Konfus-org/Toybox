#include "opengl_shadow_map_target.h"
#include <glad/glad.h>

namespace tbx::plugins
{
    bool OpenGlShadowMapTarget::try_resize(const Size& size)
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

        destroy();

        GLuint framebuffer = 0U;
        glCreateFramebuffers(1, &framebuffer);
        if (framebuffer == 0U)
        {
            return false;
        }

        GLuint depth_texture = 0U;
        glCreateTextures(GL_TEXTURE_2D, 1, &depth_texture);
        if (depth_texture == 0U)
        {
            glDeleteFramebuffers(1, &framebuffer);
            return false;
        }

        glTextureStorage2D(
            depth_texture,
            1,
            GL_DEPTH_COMPONENT32F,
            static_cast<GLsizei>(size.width),
            static_cast<GLsizei>(size.height));
        glTextureParameteri(depth_texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(depth_texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(depth_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTextureParameteri(depth_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        float border_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        glTextureParameterfv(depth_texture, GL_TEXTURE_BORDER_COLOR, border_color);

        glTextureParameteri(depth_texture, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTextureParameteri(depth_texture, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

        glNamedFramebufferTexture(framebuffer, GL_DEPTH_ATTACHMENT, depth_texture, 0);
        glNamedFramebufferDrawBuffer(framebuffer, GL_NONE);
        glNamedFramebufferReadBuffer(framebuffer, GL_NONE);

        const auto status = glCheckNamedFramebufferStatus(framebuffer, GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            glDeleteTextures(1, &depth_texture);
            glDeleteFramebuffers(1, &framebuffer);
            return false;
        }

        _framebuffer = static_cast<uint32>(framebuffer);
        _depth_texture = static_cast<uint32>(depth_texture);
        _size = size;
        return true;
    }

    void OpenGlShadowMapTarget::destroy()
    {
        if (_depth_texture != 0U)
        {
            auto id = static_cast<GLuint>(_depth_texture);
            glDeleteTextures(1, &id);
            _depth_texture = 0U;
        }

        if (_framebuffer != 0U)
        {
            auto id = static_cast<GLuint>(_framebuffer);
            glDeleteFramebuffers(1, &id);
            _framebuffer = 0U;
        }

        _size = {0U, 0U};
    }

    uint32 OpenGlShadowMapTarget::get_framebuffer() const
    {
        return _framebuffer;
    }

    uint32 OpenGlShadowMapTarget::get_depth_texture() const
    {
        return _depth_texture;
    }

    Size OpenGlShadowMapTarget::get_size() const
    {
        return _size;
    }
}

