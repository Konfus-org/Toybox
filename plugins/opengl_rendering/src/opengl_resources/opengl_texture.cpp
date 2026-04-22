#include "opengl_texture.h"
#include "opengl_bindless.h"
#include "opengl_utils.h"
#include "tbx/debugging/macros.h"
#include <algorithm>
#include <glad/glad.h>
#include <utility>

namespace opengl_rendering
{
    static uint32 take_texture_gl_handle(uint32& id) noexcept
    {
        return std::exchange(id, 0);
    }

    struct GlTextureFormat
    {
        GLenum internal_format = 0;
        GLenum data_format = 0;
    };

    static GLint calculate_mipmap_levels(const tbx::Texture& texture)
    {
        if (texture.mipmaps != tbx::TextureMipmaps::ENABLED)
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
            case tbx::TextureFilter::NEAREST:
                return texture.mipmaps == tbx::TextureMipmaps::ENABLED ? GL_NEAREST_MIPMAP_NEAREST
                                                                       : GL_NEAREST;
            case tbx::TextureFilter::LINEAR:
                return texture.mipmaps == tbx::TextureMipmaps::ENABLED ? GL_LINEAR_MIPMAP_LINEAR
                                                                       : GL_LINEAR;
            default:
                TBX_ASSERT(false, "OpenGL rendering: unsupported texture filter.");
                return GL_LINEAR;
        }
    }

    static GLenum to_gl_texture_mag_filter(tbx::TextureFilter filter)
    {
        switch (filter)
        {
            case tbx::TextureFilter::NEAREST:
                return GL_NEAREST;
            case tbx::TextureFilter::LINEAR:
                return GL_LINEAR;
            default:
                TBX_ASSERT(false, "OpenGL rendering: unsupported texture filter.");
                return GL_LINEAR;
        }
    }

    static GLenum to_gl_texture_wrap(tbx::TextureWrap wrap)
    {
        switch (wrap)
        {
            case tbx::TextureWrap::REPEAT:
                return GL_REPEAT;
            case tbx::TextureWrap::MIRRORED_REPEAT:
                return GL_MIRRORED_REPEAT;
            case tbx::TextureWrap::CLAMP_TO_EDGE:
                return GL_CLAMP_TO_EDGE;
            default:
                TBX_ASSERT(false, "OpenGL rendering: unsupported texture wrap.");
                return GL_REPEAT;
        }
    }

    static GlTextureFormat to_gl_texture_format(tbx::TextureFormat format)
    {
        switch (format)
        {
            case tbx::TextureFormat::RGBA:
                return GlTextureFormat {GL_RGBA8, GL_RGBA};
            case tbx::TextureFormat::RGB:
                return GlTextureFormat {GL_RGB8, GL_RGB};
            default:
                TBX_ASSERT(false, "OpenGL rendering: unsupported texture format.");
                return GlTextureFormat {GL_RGB8, GL_RGB};
        }
    }

    static GLenum get_compressed_internal_format(tbx::TextureFormat format)
    {
        switch (format)
        {
            case tbx::TextureFormat::RGBA:
#if defined(GL_COMPRESSED_RGBA8_ETC2_EAC)
                return GL_COMPRESSED_RGBA8_ETC2_EAC;
#else
                return 0;
#endif
            case tbx::TextureFormat::RGB:
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

    static GLenum resolve_internal_format(
        const tbx::Texture& texture,
        GLenum fallback_internal_format)
    {
        if (texture.compression == tbx::TextureCompression::DISABLED)
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
        if (texture.mipmaps == tbx::TextureMipmaps::ENABLED)
            glGenerateTextureMipmap(_texture_id);
    }

    OpenGlTexture::OpenGlTexture(const tbx::GraphicsTextureDesc& desc, const void* data)
    {
        const GLsizei width = static_cast<GLsizei>(desc.size.width);
        const GLsizei height = static_cast<GLsizei>(desc.size.height);
        const GLsizei levels = static_cast<GLsizei>(std::max(desc.mip_count, 1U));

        glCreateTextures(GL_TEXTURE_2D, 1, &_texture_id);
        glTextureStorage2D(
            _texture_id,
            levels,
            get_texture_internal_format(desc.format),
            width,
            height);

        if (data != nullptr)
        {
            glTextureSubImage2D(
                _texture_id,
                0,
                0,
                0,
                width,
                height,
                get_texture_upload_format(desc.format),
                get_texture_upload_type(desc.format),
                data);
        }

        glTextureParameteri(_texture_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(_texture_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(_texture_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_texture_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        if (desc.mip_count > 1U)
            glGenerateTextureMipmap(_texture_id);
    }

    OpenGlTexture::OpenGlTexture(OpenGlTexture&& other) noexcept
        : _texture_id(take_texture_gl_handle(other._texture_id))
        , _bindless_handle(other._bindless_handle)
    {
        other._bindless_handle = 0;
    }

    OpenGlTexture& OpenGlTexture::operator=(OpenGlTexture&& other) noexcept
    {
        if (this == &other)
            return *this;

        if (_texture_id != 0)
            glDeleteTextures(1, &_texture_id);

        if (_bindless_handle != 0)
            release_bindless_handle(_bindless_handle);

        _texture_id = take_texture_gl_handle(other._texture_id);
        _bindless_handle = other._bindless_handle;
        other._bindless_handle = 0;
        return *this;
    }

    OpenGlTexture::~OpenGlTexture() noexcept
    {
        if (_bindless_handle != 0)
        {
            release_bindless_handle(_bindless_handle);
            _bindless_handle = 0;
        }

        if (_texture_id != 0)
        {
            glDeleteTextures(1, &_texture_id);
        }
    }

    void OpenGlTexture::bind_slot(const uint32 slot) const
    {
        glBindTextureUnit(slot, _texture_id);
    }

    void OpenGlTexture::bind()
    {
        bind_slot(0U);
    }

    void OpenGlTexture::unbind()
    {
        glBindTextureUnit(0U, 0U);
    }

    uint32 OpenGlTexture::get_texture_id() const
    {
        return _texture_id;
    }

    uint64 OpenGlTexture::get_bindless_handle() const
    {
        if (_bindless_handle != 0)
            return _bindless_handle;

        auto handle = uint64 {};
        if (!try_make_bindless_handle_resident(_texture_id, handle))
            return 0;

        _bindless_handle = handle;
        return _bindless_handle;
    }

    void OpenGlTexture::update(
        const tbx::GraphicsTextureUpdateDesc& desc,
        const tbx::GraphicsTextureFormat format,
        const void* data) const
    {
        glTextureSubImage2D(
            _texture_id,
            static_cast<GLint>(desc.mip_level),
            static_cast<GLint>(desc.x),
            static_cast<GLint>(desc.y),
            static_cast<GLsizei>(desc.width),
            static_cast<GLsizei>(desc.height),
            get_texture_upload_format(format),
            get_texture_upload_type(format),
            data);
    }
}
