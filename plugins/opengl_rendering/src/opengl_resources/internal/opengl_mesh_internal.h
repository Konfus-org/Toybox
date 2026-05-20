#pragma once
#include "opengl_mesh.h"
#include "tbx/systems/debugging/macros.h"
#include <cstddef>
#include <glad/glad.h>
#include <utility>

namespace opengl_rendering::internal
{
    static void setup_instance_attributes(
        const uint32 vertex_array_id,
        const uint32 instance_buffer_id,
        const int instance_model_attribute_location,
        const int instance_id_attribute_location)
    {
        glVertexArrayVertexBuffer(
            vertex_array_id,
            1,
            instance_buffer_id,
            0,
            static_cast<GLsizei>(sizeof(OpenGlMeshInstanceData)));

        for (uint32 row = 0; row < 4; ++row)
        {
            const auto location =
                static_cast<uint32>(instance_model_attribute_location + static_cast<int>(row));
            glEnableVertexArrayAttrib(vertex_array_id, location);
            glVertexArrayAttribFormat(
                vertex_array_id,
                location,
                4,
                GL_FLOAT,
                GL_FALSE,
                static_cast<GLuint>(sizeof(float) * 4 * row));
            glVertexArrayAttribBinding(vertex_array_id, location, 1);
            glVertexArrayBindingDivisor(vertex_array_id, 1, 1);
        }

        const auto id_location = static_cast<uint32>(instance_id_attribute_location);
        glEnableVertexArrayAttrib(vertex_array_id, id_location);
        glVertexArrayAttribIFormat(
            vertex_array_id,
            id_location,
            1,
            GL_UNSIGNED_INT,
            static_cast<GLuint>(offsetof(OpenGlMeshInstanceData, instance_id)));
        glVertexArrayAttribBinding(vertex_array_id, id_location, 1);
        glVertexArrayBindingDivisor(vertex_array_id, 1, 1);
    }

    static uint32 take_mesh_gl_handle(uint32& id) noexcept
    {
        return std::exchange(id, 0);
    }

}
