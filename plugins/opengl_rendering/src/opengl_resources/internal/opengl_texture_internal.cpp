#include "opengl_texture_internal.h"

namespace opengl_rendering::internal
{
    bool has_texture_usage(
        const tbx::GraphicsTextureUsage value,
        const tbx::GraphicsTextureUsage usage)
    {
        return (static_cast<uint8>(value) & static_cast<uint8>(usage)) != 0U;
    }

    GLenum get_depth_attachment(const tbx::GraphicsTextureFormat format)
    {
        return format == tbx::GraphicsTextureFormat::DEPTH24_STENCIL8 ? GL_DEPTH_STENCIL_ATTACHMENT
                                                                      : GL_DEPTH_ATTACHMENT;
    }

    GLenum get_texture_internal_format(const tbx::GraphicsTextureFormat format)
    {
        switch (format)
        {
            case tbx::GraphicsTextureFormat::RGBA16_FLOAT:
                return GL_RGBA16F;
            case tbx::GraphicsTextureFormat::RGBA32_FLOAT:
                return GL_RGBA32F;
            case tbx::GraphicsTextureFormat::DEPTH24_STENCIL8:
                return GL_DEPTH24_STENCIL8;
            case tbx::GraphicsTextureFormat::DEPTH32_FLOAT:
                return GL_DEPTH_COMPONENT32F;
            case tbx::GraphicsTextureFormat::RGBA8:
            default:
                return GL_RGBA8;
        }
    }

    GLenum get_texture_upload_format(const tbx::GraphicsTextureFormat format)
    {
        switch (format)
        {
            case tbx::GraphicsTextureFormat::DEPTH24_STENCIL8:
                return GL_DEPTH_STENCIL;
            case tbx::GraphicsTextureFormat::DEPTH32_FLOAT:
                return GL_DEPTH_COMPONENT;
            case tbx::GraphicsTextureFormat::RGBA8:
            case tbx::GraphicsTextureFormat::RGBA16_FLOAT:
            case tbx::GraphicsTextureFormat::RGBA32_FLOAT:
            default:
                return GL_RGBA;
        }
    }

    GLenum get_texture_upload_type(const tbx::GraphicsTextureFormat format)
    {
        switch (format)
        {
            case tbx::GraphicsTextureFormat::RGBA16_FLOAT:
            case tbx::GraphicsTextureFormat::RGBA32_FLOAT:
            case tbx::GraphicsTextureFormat::DEPTH32_FLOAT:
                return GL_FLOAT;
            case tbx::GraphicsTextureFormat::DEPTH24_STENCIL8:
                return GL_UNSIGNED_INT_24_8;
            case tbx::GraphicsTextureFormat::RGBA8:
            default:
                return GL_UNSIGNED_BYTE;
        }
    }

    uint64 get_texture_byte_size(const tbx::GraphicsTextureDesc& desc)
    {
        return static_cast<uint64>(desc.size.width) * static_cast<uint64>(desc.size.height)
               * get_texture_bytes_per_pixel(desc.format);
    }

    uint64 get_texture_bytes_per_pixel(const tbx::GraphicsTextureFormat format)
    {
        switch (format)
        {
            case tbx::GraphicsTextureFormat::RGBA16_FLOAT:
                return 8U;
            case tbx::GraphicsTextureFormat::RGBA32_FLOAT:
                return 16U;
            case tbx::GraphicsTextureFormat::RGBA8:
            case tbx::GraphicsTextureFormat::DEPTH24_STENCIL8:
            case tbx::GraphicsTextureFormat::DEPTH32_FLOAT:
            default:
                return 4U;
        }
    }
}
