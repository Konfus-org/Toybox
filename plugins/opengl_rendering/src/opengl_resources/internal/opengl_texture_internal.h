#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/types/typedefs.h"
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
}
