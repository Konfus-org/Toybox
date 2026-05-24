#include "opengl_buffers_internal.h"

namespace opengl_rendering::internal
{
    bool has_buffer_usage(
        const tbx::GraphicsBufferUsage value,
        const tbx::GraphicsBufferUsage usage)
    {
        return (static_cast<uint32>(value) & static_cast<uint32>(usage)) != 0U;
    }

    GLenum to_gl_buffer_target(const tbx::GraphicsBufferUsage usage)
    {
        if (has_buffer_usage(usage, tbx::GraphicsBufferUsage::INDEX))
            return GL_ELEMENT_ARRAY_BUFFER;
        if (has_buffer_usage(usage, tbx::GraphicsBufferUsage::UNIFORM))
            return GL_UNIFORM_BUFFER;
        if (has_buffer_usage(usage, tbx::GraphicsBufferUsage::STORAGE))
            return GL_SHADER_STORAGE_BUFFER;
        if (has_buffer_usage(usage, tbx::GraphicsBufferUsage::INDIRECT_ARGS))
            return GL_DRAW_INDIRECT_BUFFER;

        return GL_ARRAY_BUFFER;
    }

    GLenum to_gl_buffer_usage(const tbx::GraphicsBufferDesc& desc)
    {
        return desc.is_dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
    }
}
