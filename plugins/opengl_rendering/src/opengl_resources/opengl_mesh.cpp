#include "opengl_mesh.h"
#include "tbx/debugging/macros.h"
#include <cstddef>
#include <glad/glad.h>
#include <utility>

namespace opengl_rendering
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

    OpenGlMesh::OpenGlMesh(const tbx::Mesh& mesh)
    {
        glCreateVertexArrays(1, &_vertex_array_id);
        glCreateBuffers(1, &_instance_buffer_id);
        set_vertex_buffer(mesh.vertices);
        set_index_buffer(mesh.indices);
    }

    OpenGlMesh::OpenGlMesh(OpenGlMesh&& other) noexcept
        : _vertex_array_id(take_mesh_gl_handle(other._vertex_array_id))
        , _instance_buffer_id(take_mesh_gl_handle(other._instance_buffer_id))
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

        _vertex_array_id = take_mesh_gl_handle(other._vertex_array_id);
        _instance_buffer_id = take_mesh_gl_handle(other._instance_buffer_id);
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
            setup_instance_attributes(
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
