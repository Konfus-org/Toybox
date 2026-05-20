#pragma once
#include "opengl_buffers.h"
#include "opengl_utils.h"
#include "tbx/systems/debugging/macros.h"
#include <algorithm>
#include <array>
#include <glad/glad.h>
#include <utility>

namespace opengl_rendering::internal
{
    static uint32 take_gl_handle(uint32& id) noexcept
    {
        return std::exchange(id, 0);
    }

    static constexpr auto GeometryPassDrawBuffers = std::array<GLenum, 7U> {
        GL_NONE,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
        GL_COLOR_ATTACHMENT3,
        GL_COLOR_ATTACHMENT4,
        GL_COLOR_ATTACHMENT5,
        GL_COLOR_ATTACHMENT6,
    };

    static void add_attribute(
        const uint32 vertex_array_id,
        const uint32 index,
        const uint32 size,
        const uint32 type,
        const uint32 offset,
        const bool normalized)
    {
        glEnableVertexArrayAttrib(vertex_array_id, index);
        if (type == GL_INT && !normalized)
            glVertexArrayAttribIFormat(vertex_array_id, index, size, type, offset);
        else
            glVertexArrayAttribFormat(
                vertex_array_id,
                index,
                size,
                type,
                normalized ? GL_TRUE : GL_FALSE,
                offset);

        glVertexArrayAttribBinding(vertex_array_id, index, 0);
    }

}
