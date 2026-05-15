#pragma once
#include "tbx/systems/graphics/pipeline/render_operation.h"
#include "tbx/systems/graphics/pipeline/commands/render_pass.h"
#include "tbx/systems/graphics/pipeline/context/render_data.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/tbx_api.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/matrices.h"
#include "tbx/types/size.h"
#include "tbx/types/uuid.h"
#include <array>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace tbx
{
    class TBX_API OpaquePassOperation final : public IRenderOperation
    {
      public:
        OpaquePassOperation(
            std::weak_ptr<IGraphicsBackend> backend,
            GraphicsResourceManager& resource_manager);
        ~OpaquePassOperation() noexcept override = default;
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(RenderData& render_data) override;
        Result execute(
            IGraphicsBackend& backend,
            RenderData& render_data,
            const CancellationToken& token) override;

      private:
        static std::vector<GraphicsResourceBinding> make_scene_uniform_bindings(
            Uuid view_uniform_buffer,
            Uuid material_uniform_buffer);
        Result ensure_material_uniform_buffer(
            IGraphicsBackend& backend,
            uint64 material_key,
            const void* data,
            uint64 data_size,
            Uuid& out_buffer);
        Result ensure_dynamic_mesh_buffers(
            IGraphicsBackend& backend,
            const std::shared_ptr<Mesh>& mesh,
            Uuid& out_vertex_buffer,
            Uuid& out_index_buffer,
            uint32& out_index_count);
        Result ensure_instance_buffer(
            IGraphicsBackend& backend,
            uint64 batch_key,
            const std::vector<Mat4>& transforms,
            Uuid& out_buffer);
        Result ensure_fallback_pipeline(IGraphicsBackend& backend);
        Result ensure_fallback_geometry_buffers(
            IGraphicsBackend& backend,
            const std::vector<float>& vertices,
            const std::vector<uint32>& indices);

      private:
        std::weak_ptr<IGraphicsBackend> _backend;
        std::reference_wrapper<GraphicsResourceManager> _resource_manager;
        std::unordered_map<uint64, Uuid> _mesh_vertex_buffers = {};
        std::unordered_map<uint64, Uuid> _mesh_index_buffers = {};
        std::unordered_map<uint64, uint32> _mesh_index_counts = {};
        std::unordered_map<uint64, std::shared_ptr<const Mesh>> _mesh_sources = {};
        std::unordered_map<uint64, Uuid> _instance_buffers = {};
        std::unordered_map<uint64, uint64> _instance_buffer_sizes = {};
        std::unordered_map<uint64, Uuid> _material_uniform_buffers = {};
        Uuid _fallback_pipeline = {};
        Uuid _fallback_vertex_buffer = {};
        Uuid _fallback_index_buffer = {};
        uint64 _fallback_vertex_buffer_size = 0U;
        uint64 _fallback_index_buffer_size = 0U;
    };

}
