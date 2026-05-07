#pragma once
#include "tbx/systems/graphics/pipeline/render_operation.h"
#include "tbx/systems/graphics/pipeline/commands/render_pass.h"
#include "tbx/tbx_api.h"
#include "tbx/types/uuid.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace tbx
{
    struct Mesh;
    struct Transform;

    /// @brief
    /// Purpose: Builds skybox draw commands from prepared render data.
    class TBX_API BuildSkyboxCommandsOperation final : public IRenderOperation
    {
      public:
        ~BuildSkyboxCommandsOperation() noexcept override = default;
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(RenderData& render_data) override;
        Result execute(
            IGraphicsBackend& backend,
            RenderData& render_data,
            const CancellationToken& token) override;
        void release(IGraphicsBackend& backend) override;

      private:
        Result ensure_geometry(IGraphicsBackend& backend);
        Result ensure_instance_buffer(
            IGraphicsBackend& backend,
            const Vec3& camera_position,
            const Transform& sky_transform);
        Result ensure_material_uniform(
            IGraphicsBackend& backend,
            uint64 material_key,
            const void* data,
            uint64 data_size);

      private:
        Uuid _vertex_buffer = {};
        Uuid _index_buffer = {};
        uint32 _index_count = 0U;
        Uuid _instance_buffer = {};
        Uuid _material_uniform_buffer = {};
        uint64 _material_key = 0U;
    };

    /// @brief
    /// Purpose: Builds opaque draw commands from visible render data items.
    class TBX_API BuildOpaqueCommandsOperation final : public IRenderOperation
    {
      public:
        ~BuildOpaqueCommandsOperation() noexcept override = default;
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(RenderData& render_data) override;
        Result execute(
            IGraphicsBackend& backend,
            RenderData& render_data,
            const CancellationToken& token) override;
        void release(IGraphicsBackend& backend) override;

      private:
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

    /// @brief
    /// Purpose: Builds alpha-cutout draw commands from visible render data items.
    class TBX_API BuildAlphaCutoutCommandsOperation final : public IRenderOperation
    {
      public:
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(RenderData& render_data) override;
        Result execute(
            IGraphicsBackend& backend,
            RenderData& render_data,
            const CancellationToken& token) override;
    };

    /// @brief
    /// Purpose: Builds transparent draw commands from visible render data items.
    class TBX_API BuildTransparentCommandsOperation final : public IRenderOperation
    {
      public:
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(RenderData& render_data) override;
        Result execute(
            IGraphicsBackend& backend,
            RenderData& render_data,
            const CancellationToken& token) override;
    };
}
