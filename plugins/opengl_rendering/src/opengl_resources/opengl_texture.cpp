#include "opengl_texture.h"
#include "tbx/debugging/macros.h"
#include <algorithm>
#include <glad/glad.h>

namespace opengl_rendering
{
    struct GlTextureFormat
    {
        GLenum internal_format = 0;
        GLenum data_format = 0;
    };

    static GLint calculate_mipmap_levels(const tbx::Texture& texture)
    {
        if (texture.mipmaps != TextureMipmaps::ENABLED)
            return 1;

        int max_dimension =
            static_cast<int>(std::max(texture.resolution.width, texture.resolution.height));
        GLint levels = 1;
        while (max_dimension > 1)
        {
            max_dimension /= 2;
            levels += 1;
        }

        return levels;
    }

    static GLenum to_gl_texture_min_filter(const tbx::Texture& texture)
    {
        switch (texture.filter)
        {
            case TextureFilter::NEAREST:
                return texture.mipmaps == TextureMipmaps::ENABLED ? GL_NEAREST_MIPMAP_NEAREST
                                                                  : GL_NEAREST;
            case TextureFilter::LINEAR:
                return texture.mipmaps == TextureMipmaps::ENABLED ? GL_LINEAR_MIPMAP_LINEAR
                                                                  : GL_LINEAR;
            default:
                TBX_ASSERT(false, "OpenGL rendering: unsupported texture filter.");
                return GL_LINEAR;
        }
    }

    static GLenum to_gl_texture_mag_filter(TextureFilter filter)
    {
        switch (filter)
        {
            case TextureFilter::NEAREST:
                return GL_NEAREST;
            case TextureFilter::LINEAR:
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
            case TextureWrap::REPEAT:
                return GL_REPEAT;
            case TextureWrap::MIRRORED_REPEAT:
                return GL_MIRRORED_REPEAT;
            case TextureWrap::CLAMP_TO_EDGE:
                return GL_CLAMP_TO_EDGE;
            default:
                TBX_ASSERT(false, "OpenGL rendering: unsupported texture wrap.");
                return GL_REPEAT;
        }
    }

    static GLenum to_gl_runtime_internal_format(OpenGlTextureRuntimeMode mode)
    {
        switch (mode)
        {
            case OpenGlTextureRuntimeMode::Color:
                return GL_RGBA8;
            case OpenGlTextureRuntimeMode::HdrColor:
                return GL_RGBA16F;
            case OpenGlTextureRuntimeMode::Depth:
                return GL_DEPTH_COMPONENT32F;
            case OpenGlTextureRuntimeMode::DepthStencil:
                return GL_DEPTH24_STENCIL8;
            default:
                TBX_ASSERT(false, "OpenGL rendering: unsupported runtime texture mode.");
                return GL_RGBA8;
        }
    }

    static GlTextureFormat to_gl_texture_format(TextureFormat format)
    {
        switch (format)
        {
            case TextureFormat::RGBA:
                return GlTextureFormat {GL_RGBA8, GL_RGBA};
            case TextureFormat::RGB:
                return GlTextureFormat {GL_RGB8, GL_RGB};
            default:
                TBX_ASSERT(false, "OpenGL rendering: unsupported texture format.");
                return GlTextureFormat {GL_RGB8, GL_RGB};
        }
    }

    static GLenum get_compressed_internal_format(TextureFormat format)
    {
        switch (format)
        {
            case TextureFormat::RGBA:
#if defined(GL_COMPRESSED_RGBA8_ETC2_EAC)
                return GL_COMPRESSED_RGBA8_ETC2_EAC;
#else
                return 0;
#endif
            case TextureFormat::RGB:
#if defined(GL_COMPRESSED_RGB8_ETC2)
                return GL_COMPRESSED_RGB8_ETC2;
#else
                return 0;
#endif
            default:
                return 0;
        }
    }

    static bool is_internal_format_supported(GLenum internal_format)
    {
        if (internal_format == 0)
            return false;

        GLint is_supported = GL_FALSE;
        glGetInternalformativ(
            GL_TEXTURE_2D,
            internal_format,
            GL_INTERNALFORMAT_SUPPORTED,
            1,
            &is_supported);
        return is_supported == GL_TRUE;
    }

    static GLenum resolve_internal_format(const tbx::Texture& texture, GLenum fallback_internal_format)
    {
        if (texture.compression == TextureCompression::DISABLED)
            return fallback_internal_format;

        const GLenum compressed = get_compressed_internal_format(texture.format);
        if (is_internal_format_supported(compressed))
            return compressed;

        TBX_TRACE_WARNING(
            "OpenGL rendering: requested texture compression is unavailable for format {}. "
            "Falling back to uncompressed upload.",
            static_cast<int>(texture.format));
        return fallback_internal_format;
    }

    OpenGlTexture::OpenGlTexture(const tbx::Texture& texture)
    {
        glCreateTextures(GL_TEXTURE_2D, 1, &_texture_id);

        const auto format = to_gl_texture_format(texture.format);
        const auto min_filter = to_gl_texture_min_filter(texture);
        const auto mag_filter = to_gl_texture_mag_filter(texture.filter);
        const auto wrapping = to_gl_texture_wrap(texture.wrap);
        const auto levels = calculate_mipmap_levels(texture);
        const auto internal_format = resolve_internal_format(texture, format.internal_format);

        glTextureParameteri(_texture_id, GL_TEXTURE_MIN_FILTER, min_filter);
        glTextureParameteri(_texture_id, GL_TEXTURE_MAG_FILTER, mag_filter);
        glTextureParameteri(_texture_id, GL_TEXTURE_WRAP_S, wrapping);
        glTextureParameteri(_texture_id, GL_TEXTURE_WRAP_T, wrapping);

        glTextureStorage2D(
            _texture_id,
            levels,
            internal_format,
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
        if (texture.mipmaps == TextureMipmaps::ENABLED)
            glGenerateTextureMipmap(_texture_id);
    }

    OpenGlTexture::OpenGlTexture(const OpenGlTextureRuntimeSettings& settings)
    {
        TBX_ASSERT(
            settings.resolution.width > 0 && settings.resolution.height > 0,
            "OpenGL rendering: runtime texture resolution must be greater than zero.");

        glCreateTextures(GL_TEXTURE_2D, 1, &_texture_id);
        if (_texture_id == 0)
            return;

        const auto gl_filter = to_gl_texture_mag_filter(settings.filter);
        auto wrapping = to_gl_texture_wrap(settings.wrap);
        if (settings.use_border_color)
            wrapping = GL_CLAMP_TO_BORDER;
        const auto internal_format = to_gl_runtime_internal_format(settings.mode);
        glTextureParameteri(_texture_id, GL_TEXTURE_MIN_FILTER, gl_filter);
        glTextureParameteri(_texture_id, GL_TEXTURE_MAG_FILTER, gl_filter);
        glTextureParameteri(_texture_id, GL_TEXTURE_WRAP_S, wrapping);
        glTextureParameteri(_texture_id, GL_TEXTURE_WRAP_T, wrapping);
        if (settings.use_border_color)
        {
            const auto border = settings.border_color;
            const float border_values[] = {border.x, border.y, border.z, border.w};
            glTextureParameterfv(_texture_id, GL_TEXTURE_BORDER_COLOR, border_values);
        }
        if (settings.use_compare_mode)
        {
            glTextureParameteri(_texture_id, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTextureParameteri(_texture_id, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        }

        glTextureStorage2D(
            _texture_id,
            1,
            internal_format,
            static_cast<GLsizei>(settings.resolution.width),
            static_cast<GLsizei>(settings.resolution.height));
    }

    OpenGlTexture::~OpenGlTexture() noexcept
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

    uint32 OpenGlTexture::get_texture_id() const
    {
        return _texture_id;
    }
}
