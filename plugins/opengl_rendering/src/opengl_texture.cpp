#include "opengl_texture.h"
#include "tbx/debugging/macros.h"
#include <glad/glad.h>

namespace tbx::plugins::openglrendering
{
    struct GlTextureFormat
    {
        GLenum internal_format = 0;
        GLenum data_format = 0;
    };

    static GLenum to_gl_texture_filter(TextureFilter filter)
    {
        switch (filter)
        {
            case TextureFilter::Nearest:
                return GL_NEAREST;
            case TextureFilter::Linear:
                return GL_LINEAR;
            default:
                TBX_ASSERT(false, "OpenGL rendering: unsupported texture filter.");
                return GL_LINEAR;
        }
    }

    static GLenum to_gl_texture_wrap(TextureWrap wrap)
    {
        switch (wrap)
        {
            case TextureWrap::Repeat:
                return GL_REPEAT;
            case TextureWrap::MirroredRepeat:
                return GL_MIRRORED_REPEAT;
            case TextureWrap::ClampToEdge:
                return GL_CLAMP_TO_EDGE;
            default:
                TBX_ASSERT(false, "OpenGL rendering: unsupported texture wrap.");
                return GL_REPEAT;
        }
    }

    static GlTextureFormat to_gl_texture_format(TextureFormat format)
    {
        switch (format)
        {
            case TextureFormat::RGBA:
                return GlTextureFormat { GL_RGBA8, GL_RGBA };
            case TextureFormat::RGB:
                return GlTextureFormat { GL_RGB8, GL_RGB };
            default:
                TBX_ASSERT(false, "OpenGL rendering: unsupported texture format.");
                return GlTextureFormat { GL_RGB8, GL_RGB };
        }
    }

    OpenGlTexture::OpenGlTexture(const Texture& texture)
    {
        glCreateTextures(GL_TEXTURE_2D, 1, &_texture_id);

        const auto format = to_gl_texture_format(texture.format);
        const auto filtering = to_gl_texture_filter(texture.filter);
        const auto wrapping = to_gl_texture_wrap(texture.wrap);

        glTextureParameteri(_texture_id, GL_TEXTURE_MIN_FILTER, filtering);
        glTextureParameteri(_texture_id, GL_TEXTURE_MAG_FILTER, filtering);
        glTextureParameteri(_texture_id, GL_TEXTURE_WRAP_S, wrapping);
        glTextureParameteri(_texture_id, GL_TEXTURE_WRAP_T, wrapping);

        glTextureStorage2D(
            _texture_id,
            1,
            format.internal_format,
            static_cast<GLsizei>(texture.resolution.width),
            static_cast<GLsizei>(texture.resolution.height));

        glTextureSubImage2D(
            _texture_id,
            0,
            0,
            0,
            static_cast<GLsizei>(texture.resolution.width),
            static_cast<GLsizei>(texture.resolution.height),
            format.data_format,
            GL_UNSIGNED_BYTE,
            texture.pixels.data());
        glGenerateTextureMipmap(_texture_id);
    }

    OpenGlTexture::~OpenGlTexture()
    {
        if (_texture_id != 0)
        {
            glDeleteTextures(1, &_texture_id);
        }
    }

    void OpenGlTexture::set_slot(uint32 slot)
    {
        _slot = slot;
    }

    void OpenGlTexture::bind()
    {
        glBindTextureUnit(_slot, _texture_id);
    }

    void OpenGlTexture::unbind()
    {
        glBindTextureUnit(_slot, 0);
    }
}
