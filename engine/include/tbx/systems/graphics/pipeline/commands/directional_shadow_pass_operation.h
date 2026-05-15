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
    class TBX_API DirectionalShadowPassOperation final : public IRenderOperation
    {
      public:
        DirectionalShadowPassOperation(
            std::weak_ptr<IGraphicsBackend> backend,
            GraphicsResourceManager& resource_manager);
        ~DirectionalShadowPassOperation() noexcept override = default;
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(RenderData& render_data) override;
        Result execute(
            IGraphicsBackend& backend,
            RenderData& render_data,
            const CancellationToken& token) override;

      private:
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
        Result ensure_shadow_pipeline(IGraphicsBackend& backend);
        Result ensure_point_shadow_pipeline(IGraphicsBackend& backend);
        Result ensure_shadow_resources(
            IGraphicsBackend& backend,
            const RenderData& frame_data,
            uint32 directional_shadow_count,
            uint32 point_shadow_count,
            uint32 spot_shadow_count,
            uint32 area_shadow_count);

      private:
        std::weak_ptr<IGraphicsBackend> _backend;
        std::reference_wrapper<GraphicsResourceManager> _resource_manager;
        std::unordered_map<uint64, Uuid> _mesh_vertex_buffers = {};
        std::unordered_map<uint64, Uuid> _mesh_index_buffers = {};
        std::unordered_map<uint64, uint32> _mesh_index_counts = {};
        std::unordered_map<uint64, Uuid> _instance_buffers = {};
        std::unordered_map<uint64, uint64> _instance_buffer_sizes = {};
        Uuid _shadow_pipeline = {};
        Uuid _point_shadow_pipeline = {};
        Uuid _directional_shadow_texture = {};
        std::vector<Uuid> _shadow_view_uniform_buffers = {};
        std::vector<Uuid> _spot_shadow_view_uniform_buffers = {};
        std::vector<Uuid> _area_shadow_view_uniform_buffers = {};
        std::vector<Uuid> _point_shadow_uniform_buffers = {};
        Uuid _point_shadow_texture = {};
        Uuid _spot_shadow_texture = {};
        Uuid _area_shadow_texture = {};
        uint32 _shadow_resolution = 0U;
        uint32 _point_shadow_texture_layers = 0U;
        uint32 _spot_shadow_texture_layers = 0U;
        uint32 _area_shadow_texture_layers = 0U;
    };

}
