#include "opengl_mesh.h"
#include "tbx/debugging/macros.h"
#include <glad/glad.h>

namespace tbx::plugins
{
    OpenGlMesh::OpenGlMesh(const Mesh& mesh)
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
        glBindVertexArray(_vertex_array_id);
        TBX_ASSERT(
            !buffer.vertices.empty(),
            "OpenGL rendering: vertex buffer must not be empty.");
        TBX_ASSERT(
            !buffer.layout.elements.empty(),
            "OpenGL rendering: vertex buffer layout must not be empty.");
        _vertex_buffer.bind();
        _vertex_buffer.upload(buffer);
    }

    void OpenGlMesh::set_index_buffer(const IndexBuffer& buffer)
    {
        glBindVertexArray(_vertex_array_id);
        TBX_ASSERT(
            !buffer.empty(),
            "OpenGL rendering: index buffer must not be empty.");
        _index_buffer.bind();
        _index_buffer.upload(buffer);
    }

    void OpenGlMesh::draw(bool draw_patches)
    {
        if (draw_patches)
        {
            glPatchParameteri(GL_PATCH_VERTICES, 3);
            glDrawElements(
                GL_PATCHES,
                static_cast<GLsizei>(_index_buffer.get_count()),
                GL_UNSIGNED_INT,
                nullptr);
            return;
        }

        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(_index_buffer.get_count()),
            GL_UNSIGNED_INT,
            nullptr);
    }

    void OpenGlMesh::bind()
    {
        glBindVertexArray(_vertex_array_id);
        _vertex_buffer.bind();
        _index_buffer.bind();
    }

    void OpenGlMesh::unbind()
    {
        glBindVertexArray(0);
        _vertex_buffer.unbind();
        _index_buffer.unbind();
    }
}
