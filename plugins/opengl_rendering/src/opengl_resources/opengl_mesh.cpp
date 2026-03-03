#include "opengl_mesh.h"
#include "tbx/debugging/macros.h"
#include <glad/glad.h>

namespace opengl_rendering
{
    OpenGlMesh::OpenGlMesh(const tbx::Mesh& mesh)
    {
        glCreateVertexArrays(1, &_vertex_array_id);
        set_vertex_buffer(mesh.vertices);
        set_index_buffer(mesh.indices);
    }

    OpenGlMesh::~OpenGlMesh() noexcept
    {
        if (_vertex_array_id != 0)
        {
            glDeleteVertexArrays(1, &_vertex_array_id);
        }
    }

    void OpenGlMesh::set_vertex_buffer(const VertexBuffer& buffer)
    {
        TBX_ASSERT(!buffer.vertices.empty(), "OpenGL rendering: vertex buffer must not be empty.");
        TBX_ASSERT(
            !buffer.layout.elements.empty(),
            "OpenGL rendering: vertex buffer layout must not be empty.");
        _vertex_buffer.upload(_vertex_array_id, buffer);
    }

    void OpenGlMesh::set_index_buffer(const IndexBuffer& buffer)
    {
        TBX_ASSERT(!buffer.empty(), "OpenGL rendering: index buffer must not be empty.");
        _index_buffer.upload(_vertex_array_id, buffer);
    }

    void OpenGlMesh::draw()
    {
        glBindVertexArray(_vertex_array_id);
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(_index_buffer.get_count()),
            GL_UNSIGNED_INT,
            nullptr);
    }

    void OpenGlMesh::draw_instanced(tbx::uint32 instance_count)
    {
        if (instance_count == 0)
            return;

        glBindVertexArray(_vertex_array_id);
        glDrawElementsInstanced(
            GL_TRIANGLES,
            static_cast<GLsizei>(_index_buffer.get_count()),
            GL_UNSIGNED_INT,
            nullptr,
            static_cast<GLsizei>(instance_count));
    }

    void OpenGlMesh::bind()
    {
        glBindVertexArray(_vertex_array_id);
    }

    void OpenGlMesh::unbind()
    {
        glBindVertexArray(0);
    }
}
