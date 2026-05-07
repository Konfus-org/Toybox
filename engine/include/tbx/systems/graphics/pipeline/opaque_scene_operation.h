#pragma once
#include "tbx/systems/graphics/pipeline/render_operation.h"
#include "tbx/systems/graphics/pipeline/render_pass.h"
#include "tbx/tbx_api.h"
#include "tbx/types/uuid.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace tbx
{
    struct Mesh;

    /// @brief
    /// Purpose: Renders all opaque scene geometry — dynamic meshes, static models, and a
    /// CPU-clipped fallback for meshes that don't satisfy the model pipeline stride.
    /// @details
    /// Ownership: Owns all transient GPU resources — per-batch instance buffers, per-mesh
    /// vertex/index buffers, per-material uniform buffers, and the fallback geometry pipeline.
    /// Resources are lazily allocated on first use and evicted after 3 frames of disuse.
    /// Interaction: Reads context.view_uniform_buffer and context.has_skybox set by earlier
    /// operations in the same prepare chain.
    class TBX_API OpaqueSceneOperation final : public IRenderOperation
    {
      public:
        OpaqueSceneOperation() = default;
        ~OpaqueSceneOperation() noexcept override = default;
        OpaqueSceneOperation(OpaqueSceneOperation&&) noexcept = default;
        OpaqueSceneOperation& operator=(OpaqueSceneOperation&&) noexcept = default;

        Result prepare(RenderFrameContext& context) override;
        Result execute(IGraphicsBackend& backend, const CancellationToken& token) override;
        void release(IGraphicsBackend& backend) override;

      private:
        // Buffer management helpers — each returns the UUID via out-param
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

        void evict_stale_resources(IGraphicsBackend& backend, uint64 frame_index);

        // Dynamic mesh geometry cache — keyed by Mesh* address
        std::unordered_map<uint64, Uuid> _mesh_vertex_buffers = {};
        std::unordered_map<uint64, Uuid> _mesh_index_buffers = {};
        std::unordered_map<uint64, uint32> _mesh_index_counts = {};
        std::unordered_map<uint64, std::shared_ptr<const Mesh>> _mesh_sources = {};
        std::unordered_map<uint64, uint64> _mesh_last_access = {};

        // Instance buffer cache — keyed by (mesh × material) or (vertex × index × material) hash
        std::unordered_map<uint64, Uuid> _instance_buffers = {};
        std::unordered_map<uint64, uint64> _instance_buffer_sizes = {};
        std::unordered_map<uint64, uint64> _instance_last_access = {};

        // Material uniform buffer cache — keyed by material content hash
        std::unordered_map<uint64, Uuid> _material_uniform_buffers = {};
        std::unordered_map<uint64, uint64> _material_uniform_last_access = {};

        // Fallback geometry resources (CPU-clipped, uploaded each frame when present)
        Uuid _fallback_pipeline = {};
        Uuid _fallback_vertex_buffer = {};
        Uuid _fallback_index_buffer = {};
        uint64 _fallback_vertex_buffer_size = 0U;
        uint64 _fallback_index_buffer_size = 0U;

        // Per-frame draw state set by prepare(), consumed by execute()
        std::vector<GraphicsIndexedDrawCommand> _draw_commands = {};
        GraphicsClearFlags _clear_flags = GraphicsClearFlags::COLOR_DEPTH;
    };
}
