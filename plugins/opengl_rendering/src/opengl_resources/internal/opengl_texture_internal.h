#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/types/typedefs.h"
#include <glad/glad.h>
#include <utility>

namespace opengl_rendering::internal
{
    static uint32 take_texture_gl_handle(uint32& id) noexcept
    {
        return std::exchange(id, 0U);
    }

    static bool is_depth_texture_format(const tbx::GraphicsTextureFormat format)
    {
        return format == tbx::GraphicsTextureFormat::DEPTH24_STENCIL8
               || format == tbx::GraphicsTextureFormat::DEPTH32_FLOAT;
    }

    bool has_texture_usage(tbx::GraphicsTextureUsage value, tbx::GraphicsTextureUsage usage);
    GLenum get_depth_attachment(tbx::GraphicsTextureFormat format);
    GLenum get_texture_internal_format(tbx::GraphicsTextureFormat format);
    GLenum get_texture_upload_format(tbx::GraphicsTextureFormat format);
    GLenum get_texture_upload_type(tbx::GraphicsTextureFormat format);
    uint64 get_texture_byte_size(const tbx::GraphicsTextureDesc& desc);
    uint64 get_texture_bytes_per_pixel(tbx::GraphicsTextureFormat format);
}
