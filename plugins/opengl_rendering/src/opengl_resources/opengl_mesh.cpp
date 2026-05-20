#include "opengl_mesh.h"
#include "internal/opengl_mesh_internal.h"
#include "tbx/systems/debugging/macros.h"
#include <cstddef>
#include <glad/glad.h>
#include <utility>
namespace opengl_rendering
{
    OpenGlMesh::OpenGlMesh(const tbx::Mesh& mesh)
    {
        glCreateVertexArrays(1, &_vertex_array_id);
        glCreateBuffers(1, &_instance_buffer_id);
        set_vertex_buffer(mesh.vertices);
        set_index_buffer(mesh.indices);
    }

    OpenGlMesh::OpenGlMesh(OpenGlMesh&& other) noexcept
        : _vertex_array_id(internal::take_mesh_gl_handle(other._vertex_array_id))
        , _instance_buffer_id(internal::take_mesh_gl_handle(other._instance_buffer_id))
        , _instance_buffer_capacity(other._instance_buffer_capacity)
        , _instance_model_attribute_location(other._instance_model_attribute_location)
        , _instance_id_attribute_location(other._instance_id_attribute_location)
        , _vertex_buffer(std::move(other._vertex_buffer))
        , _index_buffer(std::move(other._index_buffer))
    {
        other._instance_buffer_capacity = 0U;
        other._instance_model_attribute_location = -1;
        other._instance_id_attribute_location = -1;
    }

    OpenGlMesh& OpenGlMesh::operator=(OpenGlMesh&& other) noexcept
    {
        if (this == &other)
            return *this;

        if (_vertex_array_id != 0)
            glDeleteVertexArrays(1, &_vertex_array_id);
        if (_instance_buffer_id != 0)
            glDeleteBuffers(1, &_instance_buffer_id);

        _vertex_array_id = internal::take_mesh_gl_handle(other._vertex_array_id);
        _instance_buffer_id = internal::take_mesh_gl_handle(other._instance_buffer_id);
        _instance_buffer_capacity = other._instance_buffer_capacity;
        _instance_model_attribute_location = other._instance_model_attribute_location;
        _instance_id_attribute_location = other._instance_id_attribute_location;
        other._instance_buffer_capacity = 0U;
        other._instance_model_attribute_location = -1;
        other._instance_id_attribute_location = -1;
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
        if (_instance_buffer_id != 0)
        {
            glDeleteBuffers(1, &_instance_buffer_id);
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
        draw_bound();
    }

    void OpenGlMesh::draw_bound() const
    {
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(_index_buffer.get_count()),
            GL_UNSIGNED_INT,
            nullptr);
    }

    void OpenGlMesh::upload_instance_data(
        const std::vector<OpenGlMeshInstanceData>& instances,
        const int instance_model_attribute_location,
        const int instance_id_attribute_location)
    {
        if (instances.empty())
            return;
        if (_instance_buffer_id == 0)
            return;
        if (instance_model_attribute_location < 0 || instance_id_attribute_location < 0)
            return;

        const auto upload_size =
            static_cast<uint64>(instances.size()) * sizeof(OpenGlMeshInstanceData);
        if (upload_size > _instance_buffer_capacity)
        {
            glNamedBufferData(
                _instance_buffer_id,
                static_cast<GLsizeiptr>(upload_size),
                instances.data(),
                GL_STREAM_DRAW);
            _instance_buffer_capacity = upload_size;
        }
        else
        {
            glNamedBufferSubData(
                _instance_buffer_id,
                0,
                static_cast<GLsizeiptr>(upload_size),
                instances.data());
        }

        if (_instance_model_attribute_location != instance_model_attribute_location
            || _instance_id_attribute_location != instance_id_attribute_location)
        {
            internal::setup_instance_attributes(
                _vertex_array_id,
                _instance_buffer_id,
                instance_model_attribute_location,
                instance_id_attribute_location);
            _instance_model_attribute_location = instance_model_attribute_location;
            _instance_id_attribute_location = instance_id_attribute_location;
        }
    }

    void OpenGlMesh::draw_instanced(uint32 instance_count) const
    {
        if (instance_count == 0)
            return;

        glBindVertexArray(_vertex_array_id);
        draw_instanced_bound(instance_count);
    }

    void OpenGlMesh::draw_instanced_bound(const uint32 instance_count) const
    {
        if (instance_count == 0)
            return;

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
