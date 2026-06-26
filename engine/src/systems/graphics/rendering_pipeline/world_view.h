#pragma once
#include "gpu_resource_cache.h"
#include "render_validation.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/camera_view.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/frustum.h"
#include "tbx/types/matrices.h"
#include "tbx/types/size.h"
#include "tbx/utils/result.h"
#include <array>
#include <future>
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
        // Shadow casters (sky and ShadowMode::OFF excluded), concatenated in ShadowCasterCategory
        // order; each references the same `instances` buffer as draw_commands. The directional
        // cascades and every local light shadow view re-draw these same opaque casters.
        std::vector<GpuIndexedDrawCommand> shadow_draw_commands = {};
        std::array<uint32, SHADOW_CASTER_CATEGORY_COUNT> shadow_category_counts = {};
        // Per-view world -> light-clip matrices for the local (point/spot/area) light shadow atlas,
        // one entry per atlas layer (spot/area contribute one, point six). A light's
        // GpuLightData::shadow_data carries its base layer + view count into this list. Bounded by
        // MAX_LOCAL_SHADOW_VIEWS.
        std::vector<Mat4> local_shadow_matrices = {};
        // Visible draw commands for entities carrying one of the active masked tags (a subset of
        // draw_commands, referencing the same instances buffer). The pipeline draws these into the
        // tag mask that a tag-gated post effect samples.
        std::vector<GpuIndexedDrawCommand> mask_draw_commands = {};
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
        /// empty world or missing camera simply yields a result with no renderables). The returned
        /// reference borrows a buffer owned by this WorldView that is overwritten on the next
        /// capture(), so consume it before capturing again (the render lane does, synchronously).
        const WorldViewResult& capture(AssetManager& assets, World& world, GpuResourceCache& cache,
            const CameraView& camera_view, const Size& output_size, float elapsed_time,
            float light_cull_distance, float shadow_distance, float shadow_softness,
            float min_screen_size, float fade_fraction,
            const std::vector<std::string>& masked_tags = {});

      private:
        GpuMaterialData pack_material(GpuResourceCache& cache, const Material& material,
            const std::string& material_name, RenderFailure& out_failure);
        uint32 bucket_for_pipeline(GpuId pipeline, bool is_transparent, WorldViewResult& result);
        // `fade` < 0 derives the visible/shadow fade from on-screen size; >= 0 is an explicit fade the
        // LOD path supplies (its distance-based cross-fade weight). Default -1 = size-driven.
        void add_renderable(GpuResourceCache& cache, WorldViewResult& result,
            const Mat4& model_matrix, uint64 mesh_key, uint64 material_key, const Mesh& mesh,
            const Material& material, const std::string& material_name, RenderFailure forced_failure,
            bool masked, float fade = -1.0F);
        // Fade in [0,1] from projected on-screen size: 1 at/above `min_px` (sphere diameter in
        // pixels), ramping to 0 across the fade band below it (no hard cutoff). Shared by the
        // visible-geometry fade and the directional shadow-caster fade.
        float screen_size_fade(float radius, float dist, float min_px, float fade_fraction) const;
        // Returns a stable cached "material_<id>" diagnostic label, building it once per id so the
        // per-renderable name isn't reallocated every frame. The reference stays valid for the
        // WorldView's lifetime (the backing map only grows).
        const std::string& material_label(uint32 id);

      private:
        RenderValidation _validation = {};
        // Model/material asset ids that already failed to load; skip re-issuing load().
        std::unordered_set<uint32> _failed_assets = {};
        // A handle's backing asset file type never changes, so the per-slot .mti/.mat dispatch is
        // resolved once per handle id and reused — sparing a per-frame path resolve + extension
        // string allocation in the hot material loop.
        enum class SlotAssetKind
        {
            INSTANCE, // .mti — a MaterialInstance
            MATERIAL, // .mat — a base Material
            PROBE     // neither extension — probe the in-memory registered instances
        };
        std::unordered_map<uint32, SlotAssetKind> _slot_asset_kind = {};
        // Cached "material_<id>" diagnostic labels by asset id, so the per-renderable name (only read
        // on a resolve/pack failure path) isn't rebuilt from scratch every frame.
        std::unordered_map<uint32, std::string> _material_names = {};
        // Models whose async load is in flight, keyed by asset id -> the load's completion future.
        // Lets capture() render entities only once their model has streamed in (skipping them until
        // then) without re-kicking the load every frame or mistaking a slow load for a missing model.
        std::unordered_map<uint32, std::shared_future<Result>> _pending_model_loads = {};
        // Transient working set for the current capture() (reset each frame).
        Frustum _frustum = Frustum(Mat4(1.0F));
        Vec3 _camera_position = Vec3(0.0F);
        // Local-light caster reach: a surface within this camera distance stays a SOLID shadow caster
        // (fade 1) because the point/spot/area lights reuse the same caster list and need their
        // shadows regardless of the directional screen-size policy.
        float _local_light_cull_distance = 0.0F;
        // Screen-size cull/fade policy (set per capture from GraphicsSettings + the camera projection).
        // _screen_px_factor turns a world radius + camera distance into a projected pixel radius.
        bool _camera_is_perspective = true;
        float _screen_px_factor = 0.0F;
        float _min_screen_size = 0.0F;
        float _fade_fraction = 0.0F;
        // Directional shadow caster bound, set by capture()'s pre-pass: whether a caster sun exists and
        // the furthest cascade's reach. A caster beyond (dist - radius > reach) casts into no cascade,
        // so add_renderable drops it from the shadow list — a pop-free cost bound, not a visible cutoff.
        bool _has_shadow_caster = false;
        float _shadow_far_reach = 0.0F;
        std::unordered_map<GpuId, uint32> _bucket_of_pipeline = {};
        std::vector<std::vector<GpuIndexedDrawCommand>> _bucket_commands = {};
        // Whether each bucket (by creation index) blends. Blended buckets are flushed after all
        // opaque ones so transparent surfaces (which don't write depth) aren't overwritten by opaque
        // geometry behind them that happens to draw later.
        std::vector<bool> _bucket_transparent = {};
        // Per-category shadow caster draw commands for the current capture(), flattened at the end.
        std::array<std::vector<GpuIndexedDrawCommand>, SHADOW_CASTER_CATEGORY_COUNT>
            _shadow_commands = {};
        // Reused across frames so capture() reuses each vector's storage instead of reallocating the
        // whole result (instances, lights, draw commands, buckets…) every frame. Cleared, not
        // freed, at the start of each capture().
        WorldViewResult _result = {};
    };
}
