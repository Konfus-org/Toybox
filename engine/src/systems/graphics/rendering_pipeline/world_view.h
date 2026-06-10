#pragma once
#include "gpu_resource_cache.h"
#include "render_validation.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/frustum.h"
#include "tbx/types/matrices.h"
#include "tbx/types/size.h"
#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tbx
{
    /// @brief Shadow caster classification. Indexes WorldViewResult::shadow_category_counts and the
    /// concatenation order of shadow_draw_commands. Opaque casters write the depth shadow map;
    /// transparent casters write the transmittance (color) map. Two-sided variants disable culling.
    enum ShadowCasterCategory : uint32
    {
        SHADOW_CASTER_OPAQUE_ONE_SIDED = 0U,
        SHADOW_CASTER_OPAQUE_TWO_SIDED = 1U,
        SHADOW_CASTER_TRANSPARENT_ONE_SIDED = 2U,
        SHADOW_CASTER_TRANSPARENT_TWO_SIDED = 3U,
        SHADOW_CASTER_CATEGORY_COUNT = 4U,
    };

    /// @brief A camera's view of the world for one frame: the CPU-side arrays the pipeline uploads.
    struct WorldViewResult
    {
        GpuUniforms uniforms = {};
        std::vector<GpuInstanceData> instances = {};
        std::vector<GpuLightData> lights = {};
        std::vector<GpuIndexedDrawCommand> draw_commands = {}; // concatenated in bucket order
        std::vector<GpuId> bucket_pipelines = {}; // one raster pipeline per draw bucket
        std::vector<uint32> bucket_command_counts = {}; // draw count per bucket (aligned)
        // Directional shadow casters only (sky and ShadowMode::OFF excluded), concatenated in
        // ShadowCasterCategory order; each references the same `instances` buffer as draw_commands.
        std::vector<GpuIndexedDrawCommand> shadow_draw_commands = {};
        std::array<uint32, SHADOW_CASTER_CATEGORY_COUNT> shadow_category_counts = {};
        bool has_camera = false;
    };

    /// @brief
    /// Purpose: Produces a camera's view of the active world each frame. It walks the world, frustum
    /// culls renderables on the CPU, resolves their geometry/materials/textures/pipelines through
    /// the GpuResourceCache, and emits the per-instance + light arrays, the per-bucket indexed draw
    /// commands, and the camera-derived scene uniforms — all as plain CPU data for the pipeline to
    /// upload. It owns no GPU resources.
    /// @details
    /// Holds only CPU bookkeeping: a warn-once set and a negative cache of failed model/material
    /// asset ids. Every resolve failure falls back to a pinned magenta built-in and warns once.
    /// Thread Safety: Render-lane only.
    class WorldView final
    {
      public:
        WorldView(const WorldView&) = delete;
        WorldView& operator=(const WorldView&) = delete;
        WorldView() = default;

      public:
        /// @brief Builds this frame's render view from the camera's perspective (never fails — an
        /// empty world or missing camera simply yields a result with no renderables).
        WorldViewResult capture(AssetManager& assets, World& world, GpuResourceCache& cache,
            const Size& output_size, float elapsed_time, float light_cull_distance,
            float shadow_distance, float shadow_softness);

      private:
        GpuMaterialData pack_material(GpuResourceCache& cache, const Material& material,
            const std::string& material_name, RenderFailure& out_failure);
        uint32 bucket_for_pipeline(GpuId pipeline, WorldViewResult& result);
        void add_renderable(GpuResourceCache& cache, WorldViewResult& result,
            const Mat4& model_matrix, uint64 mesh_key, uint64 material_key, const Mesh& mesh,
            const Material& material, const std::string& material_name, RenderFailure forced_failure);

      private:
        RenderValidation _validation = {};
        // Model/material asset ids that already failed to load; skip re-issuing load().
        std::unordered_set<uint32> _failed_assets = {};
        // Transient working set for the current capture() (reset each frame).
        Frustum _frustum = Frustum(Mat4(1.0F));
        std::unordered_map<GpuId, uint32> _bucket_of_pipeline = {};
        std::vector<std::vector<GpuIndexedDrawCommand>> _bucket_commands = {};
        // Per-category shadow caster draw commands for the current capture(), flattened at the end.
        std::array<std::vector<GpuIndexedDrawCommand>, SHADOW_CASTER_CATEGORY_COUNT>
            _shadow_commands = {};
    };
}
