#pragma once
#include "opengl_bindless.h"
#include "opengl_texture.h"
#include "opengl_utils.h"
#include "tbx/systems/debugging/macros.h"
#include <algorithm>
#include <glad/glad.h>
#include <utility>

namespace opengl_rendering::internal
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

    static bool is_depth_texture_format(const tbx::GraphicsTextureFormat format)
    {
        return format == tbx::GraphicsTextureFormat::DEPTH24_STENCIL8
               || format == tbx::GraphicsTextureFormat::DEPTH32_FLOAT;
    }

}
