#include "tbx/types/components/mesh.h"
#include "tbx/types/mesh_bounds.h"
#include "types/components/internal/mesh_internal.h"
#include <utility>

namespace tbx
{
    Mesh::Mesh()
    {
        const Mesh& default_quad = Mesh::QUAD;
        vertices = default_quad.vertices;
        indices = default_quad.indices;
        bounds = default_quad.bounds;
    }

    Mesh::Mesh(VertexBuffer vert_buff, IndexBuffer index_buff)
        : vertices(std::move(vert_buff))
        , indices(std::move(index_buff))
    {
        update_mesh_bounds(*this);
    }

    uint32 Mesh::get_vertex_stride_float_count() const
    {
        const uint32 stride_bytes = vertices.layout.stride;
        return stride_bytes == 0U ? 16U : stride_bytes / static_cast<uint32>(sizeof(float));
    }

    DynamicMesh::DynamicMesh(Mesh mesh)
        : _data(std::make_shared<DynamicMeshData>(std::move(mesh)))
    {
    }

    DynamicMesh::DynamicMesh(std::shared_ptr<DynamicMeshData> mesh_data)
        : _data(std::move(mesh_data))
    {
    }

    const Mesh& DynamicMesh::get_mesh() const
    {
        static const Mesh EMPTY_MESH = Mesh(VertexBuffer {}, IndexBuffer {});
        return _data ? _data->get_mesh() : EMPTY_MESH;
    }

    Mesh& DynamicMesh::edit_mesh()
    {
        if (!_data)
            _data = std::make_shared<DynamicMeshData>();

        return _data->edit_mesh();
    }

    bool DynamicMesh::is_dirty() const
    {
        return _data && _data->is_dirty();
    }

    void DynamicMesh::mark_dirty()
    {
        if (_data)
            _data->mark_dirty();
    }

    void DynamicMesh::clear_dirty()
    {
        if (_data)
            _data->clear_dirty();
    }

    std::shared_ptr<DynamicMeshData> DynamicMesh::get_data() const
    {
        return _data;
    }

    DynamicMeshData::DynamicMeshData(Mesh mesh)
        : _mesh(std::move(mesh))
    {
    }

    const Mesh& DynamicMeshData::get_mesh() const
    {
        return _mesh;
    }

    Mesh& DynamicMeshData::edit_mesh()
    {
        mark_dirty();
        return _mesh;
    }

    bool DynamicMeshData::is_dirty() const
    {
        return _is_dirty;
    }

    void DynamicMeshData::mark_dirty()
    {
        _is_dirty = true;
    }

    void DynamicMeshData::clear_dirty()
    {
        _is_dirty = false;
    }

    const Mesh Mesh::TRIANGLE = internal::make_triangle_mesh();
    const Mesh Mesh::QUAD = internal::make_quad_mesh();
    const Mesh Mesh::FULLSCREEN_QUAD = internal::make_fullscreen_quad_mesh();
    const Mesh Mesh::CUBE = internal::make_cube_mesh();
    const Mesh Mesh::SPHERE = internal::make_sphere_mesh();
    const Mesh Mesh::CAPSULE = internal::make_capsule_default_mesh();
    const Mesh Mesh::HALF_SPHERE = internal::make_half_sphere_mesh();
}
