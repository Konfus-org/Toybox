#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/types/typedefs.h"
#include <glad/glad.h>
#include <utility>

namespace opengl_rendering::internal
{
    static uint32 take_buffer_gl_handle(uint32& id) noexcept
    {
        return std::exchange(id, 0U);
    }

    bool has_buffer_usage(tbx::GraphicsBufferUsage value, tbx::GraphicsBufferUsage usage);
    GLenum to_gl_buffer_target(tbx::GraphicsBufferUsage usage);
    GLenum to_gl_buffer_usage(const tbx::GraphicsBufferDesc& desc);
}
