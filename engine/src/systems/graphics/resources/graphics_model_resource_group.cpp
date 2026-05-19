#include "graphics_model_resource_group.h"
#include "graphics_buffer_resource.h"
#include "tbx/systems/debugging/macros.h"
#include <string>
#include <utility>

namespace tbx::detail
{
    GraphicsModelResourceGroup::GraphicsModelResourceGroup(
        std::weak_ptr<IGraphicsBackend> backend,
        std::vector<std::shared_ptr<GraphicsResource>> mesh_resources,
        std::vector<GraphicsModelMeshResource> meshes)
        : GraphicsResource(std::move(backend), Uuid::generate())
        , _mesh_resources(std::move(mesh_resources))
        , _meshes(std::move(meshes))
    {
        update_mesh_resource_uuids();
    }

    const std::vector<GraphicsModelMeshResource>& GraphicsModelResourceGroup::get_meshes() const
    {
        return _meshes;
    }

    GraphicsModelResourceGroup::GraphicsModelResourceGroup(
        std::weak_ptr<IGraphicsBackend> backend,
        const Handle& handle,
        const Model& model)
        : GraphicsResource(std::move(backend), Uuid::generate())
    {
        auto mesh_resources = std::vector<std::shared_ptr<GraphicsResource>> {};
        auto meshes = std::vector<GraphicsModelMeshResource> {};

        const auto add_mesh_resources =
            [&](const Mesh& mesh, const uint mesh_index) -> bool
        {
            auto mesh_resource = GraphicsModelMeshResource {
                .index_count = static_cast<uint32>(mesh.indices.size()),
            };

            if (mesh.bounds.is_valid)
            {
                mesh_resource.local_bounds = mesh.bounds.sphere;
                mesh_resource.has_local_bounds = true;
            }

            auto pending_resources = std::vector<std::shared_ptr<GraphicsResource>> {};

            if (mesh.vertices.empty())
            {
                TBX_TRACE_ERROR_ONCE(
                    "Graphics model resource ({}) handle '{}' failed to create vertex buffer for mesh "
                    "index {}: source mesh has no vertex data.",
                    to_string(get_uuid()),
                    to_string(handle),
                    mesh_index);
                return false;
            }

            const uint64 vertex_data_size =
                static_cast<uint64>(mesh.vertices.size()) * static_cast<uint64>(sizeof(float));
            const auto vertex_buffer_resource =
                std::make_shared<GraphicsBufferResource>(
                    backend,
                    make_model_vertex_buffer_desc(handle, mesh_index, vertex_data_size),
                    std::vector<uint8>(
                        reinterpret_cast<const uint8*>(mesh.vertices.data()),
                        reinterpret_cast<const uint8*>(mesh.vertices.data())
                            + static_cast<size>(vertex_data_size)));
            if (!vertex_buffer_resource || !vertex_buffer_resource->get_uuid().is_valid())
            {
                TBX_TRACE_ERROR_ONCE(
                    "Graphics model resource ({}) handle '{}': failed to create vertex buffer for mesh "
                    "index {}.",
                    to_string(get_uuid()),
                    to_string(handle),
                    mesh_index);
                return false;
            }

            mesh_resource.vertex_buffer = Uuid::generate();
            pending_resources.push_back(std::move(vertex_buffer_resource));

            if (mesh.indices.empty())
            {
                TBX_TRACE_ERROR_ONCE(
                    "Graphics model resource ({}) handle '{}': failed to create index buffer for mesh "
                    "index {}: source mesh has no indices.",
                    to_string(get_uuid()),
                    to_string(handle),
                    mesh_index);
                return false;
            }

            const uint64 index_data_size =
                static_cast<uint64>(mesh.indices.size()) * static_cast<uint64>(sizeof(uint32));
            const auto index_buffer_resource =
                std::make_shared<GraphicsBufferResource>(
                    backend,
                    make_model_index_buffer_desc(handle, mesh_index, index_data_size),
                    std::vector<uint8>(
                        reinterpret_cast<const uint8*>(mesh.indices.data()),
                        reinterpret_cast<const uint8*>(mesh.indices.data())
                            + static_cast<size>(index_data_size)));
            if (!index_buffer_resource || !index_buffer_resource->get_uuid().is_valid())
            {
                TBX_TRACE_ERROR_ONCE(
                    "Graphics model resource ({}) handle '{}': failed to create index buffer for mesh "
                    "index {}.",
                    to_string(get_uuid()),
                    to_string(handle),
                    mesh_index);
                return false;
            }

            mesh_resource.index_buffer = Uuid::generate();
            pending_resources.push_back(std::move(index_buffer_resource));

            if (!mesh_resource.vertex_buffer.is_valid()
                || !mesh_resource.index_buffer.is_valid()
                || mesh_resource.index_count == 0U)
            {
                return false;
            }

            for (auto& resource : pending_resources)
                mesh_resources.push_back(std::move(resource));
            meshes.push_back(mesh_resource);
            return true;
        };

        for (uint mesh_index = 0U; mesh_index < static_cast<uint>(model.meshes.size()); ++mesh_index)
        {
            const Mesh& mesh = model.meshes[static_cast<size>(mesh_index)];
            if (!add_mesh_resources(mesh, mesh_index))
            {
                TBX_TRACE_WARNING_ONCE(
                    "Graphics model resource ({}) handle '{}': mesh index {} failed to upload. "
                    "Falling back to cube mesh.",
                    to_string(get_uuid()),
                    to_string(handle),
                    mesh_index);
                if (!add_mesh_resources(make_cube(), mesh_index))
                {
                    TBX_TRACE_ERROR_ONCE(
                        "Graphics model resource ({}) handle '{}': fallback cube mesh failed for mesh "
                        "index {}.",
                        to_string(get_uuid()),
                        to_string(handle),
                        mesh_index);
                }
            }
        }

        if (meshes.empty())
        {
            TBX_TRACE_WARNING_ONCE(
                "Graphics model resource ({}) handle '{}': model has no mesh data to upload. "
                "Falling back to cube mesh.",
                to_string(get_uuid()),
                to_string(handle));
            if (!add_mesh_resources(make_cube(), 0U))
            {
                TBX_TRACE_ERROR_ONCE(
                    "Graphics model resource ({}) handle '{}': fallback cube mesh failed.",
                    to_string(get_uuid()),
                    to_string(handle));
            }
        }

        _mesh_resources = std::move(mesh_resources);
        _meshes = std::move(meshes);
        update_mesh_resource_uuids();
    }

    GraphicsBufferDesc GraphicsModelResourceGroup::make_model_index_buffer_desc(
        const Handle& handle,
        const uint mesh_index,
        const uint64 byte_size)
    {
        return GraphicsBufferDesc {
            .usage = GraphicsBufferUsage::INDEX,
            .size = byte_size,
            .is_dynamic = false,
            .debug_name = std::string("Model ") + to_string(handle) + " Mesh "
                          + std::to_string(mesh_index) + " Indices",
        };
    }

    GraphicsBufferDesc GraphicsModelResourceGroup::make_model_vertex_buffer_desc(
        const Handle& handle,
        const uint mesh_index,
        const uint64 byte_size)
    {
        return GraphicsBufferDesc {
            .usage = GraphicsBufferUsage::VERTEX,
            .size = byte_size,
            .is_dynamic = false,
            .debug_name = std::string("Model ") + to_string(handle) + " Mesh "
                          + std::to_string(mesh_index) + " Vertices",
        };
    }

    void GraphicsModelResourceGroup::update_mesh_resource_uuids()
    {
        auto resource_index = size {0U};
        for (auto& mesh : _meshes)
        {
            if (mesh.vertex_buffer.is_valid() && resource_index < _mesh_resources.size())
            {
                mesh.vertex_buffer = _mesh_resources[resource_index]->get_uuid();
                resource_index += 1U;
            }

            if (mesh.index_buffer.is_valid() && resource_index < _mesh_resources.size())
            {
                mesh.index_buffer = _mesh_resources[resource_index]->get_uuid();
                resource_index += 1U;
            }
        }
    }
}

