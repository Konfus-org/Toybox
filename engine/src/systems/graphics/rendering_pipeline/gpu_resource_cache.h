#pragma once
#include "gpu_resources.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/assets/shader.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/handle.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/result.h"
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tbx
{
    // GPU resources untouched for longer than the grace window are purged on the eviction interval.
    constexpr double GPU_RESOURCE_IDLE_GRACE_SECONDS = 10.0;
    constexpr double GPU_RESOURCE_EVICTION_INTERVAL_SECONDS = 2.0;

    /// @brief Stable key the cache addresses every stored resource by (caller-supplied). Distinct
    /// from GpuId (a backend resource handle) and from the uint32 SSBO slot indices the cache hands
    /// back: it is purely the lookup key for add/get/has/remove.
    using CacheId = uint64;

    // Fixed CacheIds for the cache's four persistent mega-buffers (retrieved via get_buffer).
    constexpr CacheId VERTICES_BUFFER_ID = 0xB0FFE40000000001ULL;
    constexpr CacheId INDICES_BUFFER_ID = 0xB0FFE40000000002ULL;
    constexpr CacheId MATERIAL_TABLE_BUFFER_ID = 0xB0FFE40000000003ULL;
    constexpr CacheId TEXTURE_TABLE_BUFFER_ID = 0xB0FFE40000000004ULL;

    /// @brief One mesh's place in the geometry mega-buffer (CPU-side; ranges feed indexed draws).
    struct GpuMeshRecord
    {
        GpuMeshData data = {};
        GpuId mesh_id = 0U;
        GpuBufferRange vertex_range = {};
        GpuBufferRange index_range = {};
        bool is_pinned = false;
        double last_used = 0.0;
    };

    /// @brief One material's slot in the material table. last_data caches the bytes last uploaded to
    /// that slot so add_material can skip the GPU write_buffer when a re-registered material is
    /// unchanged (the common per-frame case: the same material re-emitted every frame, often by many
    /// instances) — the slot already holds the data.
    struct GpuMaterialRecord
    {
        GpuId material_id = 0U;
        bool is_pinned = false;
        double last_used = 0.0;
        GpuMaterialData last_data = {};
        bool has_data = false;
    };

    /// @brief One texture's bindless slot + the RAII handle that owns the GPU texture.
    struct GpuTextureRecord
    {
        GpuResource texture = {};
        uint32 index = 0U;
        bool is_pinned = false;
        double last_used = 0.0;
    };

    /// @brief One compiled raster pipeline + the RAII handle that owns it. An invalid handle marks
    /// a known compile failure cached so it is not retried every frame (until it idles out).
    struct GpuPipelineRecord
    {
        GpuResource pipeline = {};
        bool is_pinned = false;
        double last_used = 0.0;
    };

    /// @brief
    /// Purpose: Caches every persistent GPU resource the forward+ renderer reuses across frames —
    /// the vertex/index geometry mega-buffer, the packed material table, the bindless texture table
    /// (uploaded from texture assets), the compiled raster pipelines, and the pinned magenta
    /// fallbacks. Entries are keyed by a stable id and reference-counted by last-use.
    /// @details
    /// The cache owns its own clock: update() advances time and evicts idle entries, all privately
    /// — callers never "ensure" buffers or track changes. Uploads happen immediately on
    /// registration. Memory discipline: stores only GPU-facing records; source mesh/pixel payloads
    /// are released after upload. Pools are fixed capacity and never relocate; freed ranges return
    /// to a free list. Ownership: Owns (RAII) every GpuId it allocates. Thread Safety: Render-lane
    /// only.
    class GpuResourceCache final
    {
      public:
        GpuResourceCache(
            std::weak_ptr<IGraphicsBackend> backend,
            std::weak_ptr<AssetManager> assets);
        ~GpuResourceCache();

        GpuResourceCache(const GpuResourceCache&) = delete;
        GpuResourceCache& operator=(const GpuResourceCache&) = delete;

      public:
        /// @brief Advances the cache clock and evicts unpinned resources idle past the grace
        /// window.
        void update(const DeltaTime& delta_time);

        std::optional<GpuMesh> add_mesh(CacheId id, const Mesh& mesh, bool pinned);
        std::optional<GpuId> add_material(CacheId id, const GpuMaterialData& material, bool pinned);
        std::optional<GpuId> add_texture(CacheId id, const Handle& handle, bool pinned);
        std::optional<GpuId> add_texture(
            CacheId id,
            const TextureDesc& desc,
            const void* pixels,
            size pixels_size,
            bool pinned);
        std::optional<GpuId> add_pipeline(
            CacheId id,
            const ShaderProgram& shader,
            const RasterState& state,
            bool pinned);

        std::optional<GpuMesh> get_mesh(CacheId id);
        std::optional<GpuId> get_material(CacheId id);
        std::optional<GpuId> get_texture(CacheId id);
        std::optional<GpuId> get_pipeline(CacheId id);
        std::optional<GpuId> get_buffer(CacheId id) const;

        /// @brief True if any store currently holds this id.
        bool has(CacheId id) const;

        /// @brief Drops the resource with this id (freeing its slot/range), if present.
        void remove(CacheId id);

      private:
        Result ensure_ready();
        void evict_unreferenced();
        void free_mesh(const GpuMeshRecord& record);
        void free_material(const GpuMaterialRecord& record);
        void free_texture(const GpuTextureRecord& record);

      private:
        std::weak_ptr<IGraphicsBackend> _backend = {};
        std::weak_ptr<AssetManager> _assets = {};
        double _now_seconds = 0.0;
        double _last_eviction = 0.0;
        bool _is_ready = false;

        // Geometry mega-buffer (vertices SSBO + index element buffer). Mesh ranges are CPU-side.
        std::unordered_map<CacheId, GpuMeshRecord> _meshes = {};
        std::vector<GpuBufferRange> _free_vertex_ranges = {};
        std::vector<GpuBufferRange> _free_index_ranges = {};
        uint32 _vertex_bump = 0U;
        uint32 _index_bump = 0U;
        uint32 _mesh_id_bump = 0U;
        std::vector<uint32> _free_mesh_ids = {};
        GpuResource _vertices = {};
        GpuResource _indices = {};

        // Material table.
        std::unordered_map<CacheId, GpuMaterialRecord> _materials = {};
        uint32 _material_count = 0U;
        std::vector<uint32> _free_material_ids = {};
        GpuResource _material_table = {};

        // Bindless texture table (uploaded from assets or raw pixels).
        std::unordered_map<CacheId, GpuTextureRecord> _textures = {};
        uint32 _texture_bump = 0U;
        std::vector<uint32> _free_texture_indices = {};
        std::unordered_set<CacheId> _failed_textures = {};
        GpuResource _texture_table = {};

        // Compiled raster pipelines (keyed by shader + render state).
        std::unordered_map<CacheId, GpuPipelineRecord> _pipelines = {};
    };
}
