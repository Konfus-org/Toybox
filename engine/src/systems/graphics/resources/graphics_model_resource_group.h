#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/graphics_resource.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/types/components/model.h"
#include <memory>
#include <vector>

namespace tbx::detail
{
        class GraphicsModelResourceGroup final : public GraphicsResource
        {
          public:
        GraphicsModelResourceGroup(
            std::weak_ptr<IGraphicsBackend> backend,
            std::vector<std::shared_ptr<GraphicsResource>> mesh_resources,
            std::vector<GraphicsModelMeshResource> meshes);
        GraphicsModelResourceGroup(
            std::weak_ptr<IGraphicsBackend> backend,
            const Handle& handle,
            const Model& model);
        ~GraphicsModelResourceGroup() noexcept override = default;

          public:
            const std::vector<GraphicsModelMeshResource>& get_meshes() const;

          private:
            static GraphicsBufferDesc make_model_index_buffer_desc(
                const Handle& handle,
                uint mesh_index,
                uint64 byte_size);
            static GraphicsBufferDesc make_model_vertex_buffer_desc(
                const Handle& handle,
                uint mesh_index,
                uint64 byte_size);
            void update_mesh_resource_uuids();

          private:
            std::vector<std::shared_ptr<GraphicsResource>> _mesh_resources = {};
            std::vector<GraphicsModelMeshResource> _meshes = {};
        };
}
