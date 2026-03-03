#include "opengl_mesh.h"
#include "tbx/debugging/macros.h"
#include <glad/glad.h>
#include <utility>

namespace opengl_rendering
{
    static tbx::uint32 take_gl_handle(tbx::uint32& id) noexcept
    {
        return std::exchange(id, 0);
    }

    OpenGlMesh::OpenGlMesh(const tbx::Mesh& mesh)
    {
        glCreateVertexArrays(1, &_vertex_array_id);
        set_vertex_buffer(mesh.vertices);
        set_index_buffer(mesh.indices);
    }

    OpenGlMesh::OpenGlMesh(OpenGlMesh&& other) noexcept
        : _vertex_array_id(take_gl_handle(other._vertex_array_id))
        , _vertex_buffer(std::move(other._vertex_buffer))
        , _index_buffer(std::move(other._index_buffer))
    {
    }

    OpenGlMesh& OpenGlMesh::operator=(OpenGlMesh&& other) noexcept
    {
        if (this == &other)
            return *this;

        if (_vertex_array_id != 0)
            glDeleteVertexArrays(1, &_vertex_array_id);

        _vertex_array_id = take_gl_handle(other._vertex_array_id);
        _vertex_buffer = std::move(other._vertex_buffer);
        _index_buffer = std::move(other._index_buffer);
        return *this;
    }

    OpenGlMesh::~OpenGlMesh() noexcept
    {
        if (_vertex_array_id != 0)
        {
            glDeleteVertexArrays(1, &_vertex_array_id);
        }
    }

    void OpenGlMesh::set_vertex_buffer(const tbx::VertexBuffer& buffer)
    {
        TBX_ASSERT(!buffer.vertices.empty(), "OpenGL rendering: vertex buffer must not be empty.");
        TBX_ASSERT(
            !buffer.layout.elements.empty(),
            "OpenGL rendering: vertex buffer layout must not be empty.");
        _vertex_buffer.upload(_vertex_array_id, buffer);
    }

    void OpenGlMesh::set_index_buffer(const tbx::IndexBuffer& buffer)
    {
        TBX_ASSERT(!buffer.empty(), "OpenGL rendering: index buffer must not be empty.");
        _index_buffer.upload(_vertex_array_id, buffer);
    }

    void OpenGlMesh::draw() const
    {
        glBindVertexArray(_vertex_array_id);
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(_index_buffer.get_count()),
            GL_UNSIGNED_INT,
            nullptr);
    }

    void OpenGlMesh::draw_instanced(tbx::uint32 instance_count) const
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
