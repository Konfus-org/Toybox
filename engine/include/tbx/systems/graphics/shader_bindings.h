#pragma once
#include "tbx/types/matrices.h"
#include "tbx/types/typedefs.h"
#include <array>

namespace tbx
{
    // IMPORTANT: KEEP THESE BINDINGS AND SHADER STRUCTS IN SYNC WITH SHADERS!

    // ----------------------------------------------------
    // Unbound Bindless Resource Table Slot Bindings
    // ----------------------------------------------------
    constexpr uint32 GPU_BINDING_ALL_INSTANCES = 0U;
    constexpr uint32 GPU_BINDING_GLOBAL_VERTICES = 1U;
    constexpr uint32 GPU_BINDING_GLOBAL_MESHES = 2U;
    constexpr uint32 GPU_BINDING_GLOBAL_MATERIALS = 3U;
    constexpr uint32 GPU_BINDING_GLOBAL_LIGHTS = 4U;
    constexpr uint32 GPU_BINDING_CLUSTER_GRID = 5U;
    constexpr uint32 GPU_BINDING_LIGHT_INDEX_POOL = 6U;
    constexpr uint32 GPU_BINDING_DRAW_ARGS = 7U;
    constexpr uint32 GPU_BINDING_SHADOW_ARGS_POOL = 8U;
    constexpr uint32 GPU_BINDING_DRAW_COUNT = 9U;
    constexpr uint32 GPU_BINDING_SHADOW_DRAW_COUNT = 10U;
    constexpr uint32 GPU_BINDING_GLOBAL_TEXTURES = 11U;

    // Number of directional shadow cascades (distance-split shadow maps). Cascade 0 is the
    // highest-resolution near slice; the furthest cascade is the lowest resolution and reaches the
    // configured shadow_render_distance. KEEP IN SYNC with TBX_SHADER_CASCADE_COUNT in ShaderBase.glsl.
    constexpr uint32 SHADOW_CASCADE_COUNT = 4U;

    // Directional shadow cascade depth maps occupy consecutive sampler units
    // [GPU_BINDING_SHADOW_CASCADE_BASE, +SHADOW_CASCADE_COUNT). Each is a depth texture sampled by the
    // forward pass, which selects one per fragment by camera distance. Mirrors
    // TBX_SHADER_BINDING_SHADOW_CASCADE_0 in ShaderBase.glsl.
    constexpr uint32 GPU_BINDING_SHADOW_CASCADE_BASE = 12U;
    // Directional translucent (colored) shadow maps: one RGBA transmittance texture per cascade, each
    // rendered/sampled with its own cascade's projection + depth so transparent casters cast a colored,
    // partial shadow at every distance (matching the per-cascade depth maps). They occupy the
    // consecutive sampler units [GPU_BINDING_SHADOW_COLOR_BASE, +SHADOW_CASCADE_COUNT). Mirrors
    // TBX_SHADER_BINDING_SHADOW_COLOR_0 in ShaderBase.glsl.
    constexpr uint32 GPU_BINDING_SHADOW_COLOR_BASE = GPU_BINDING_SHADOW_CASCADE_BASE + SHADOW_CASCADE_COUNT;

    // Local (point/spot/area) light shadows share ONE depth texture-array atlas: each shadow-casting
    // local light occupies a contiguous run of layers — one for a spot/area light, six (cube faces)
    // for a point light. The forward pass projects the fragment by the light's per-view matrix
    // (localShadowMatrices[]) and depth-compares against the matching layer so the light no longer
    // bleeds through walls. MAX_LOCAL_SHADOW_VIEWS bounds the array's layer count (and so the atlas
    // VRAM); lights past the budget are still lit, just unshadowed. KEEP IN SYNC with ShaderBase.glsl.
    constexpr uint32 MAX_LOCAL_SHADOW_VIEWS = 16U;
    // sampler2DArray of per-view depth maps, one consecutive texture unit after the directional color
    // cascades. Mirrors TBX_SHADER_BINDING_LOCAL_SHADOW_ATLAS in ShaderBase.glsl.
    constexpr uint32 GPU_BINDING_LOCAL_SHADOW_ATLAS = GPU_BINDING_SHADOW_COLOR_BASE + SHADOW_CASCADE_COUNT;

    constexpr uint32 GPU_BINDING_UNIFORMS = 0U;
    // Per-view world -> light-clip matrices for local light shadows, indexed by atlas layer. Bound as
    // an SSBO in the world group (reuses the reserved-but-unused global meshes slot 2 — the renderer
    // bakes mesh ranges into draw commands, so nothing binds an SSBO there). Mirrors
    // TBX_SHADER_BINDING_LOCAL_SHADOW_MATRICES in ShaderBase.glsl.
    constexpr uint32 GPU_BINDING_LOCAL_SHADOW_MATRICES = GPU_BINDING_GLOBAL_MESHES;
    // Per-cascade caster UBO: the shadow depth/color caster pass reads the active cascade's
    // world -> light-clip matrix from here. Mirrors TBX_SHADER_BINDING_SHADOW_PASS in ShaderBase.glsl.
    constexpr uint32 GPU_BINDING_SHADOW_PASS_UNIFORMS = 6U;
    // Post-processing: per-effect uniforms (UBO) + the scene color sampled by a fullscreen effect.
    // GPU_BINDING_SCENE_COLOR mirrors TBX_SHADER_BINDING_FINAL_HDR (23) in ShaderBase.glsl.
    constexpr uint32 GPU_BINDING_POST_UNIFORMS = 5U;
    constexpr uint32 GPU_BINDING_SCENE_COLOR = 23U;

    // ----------------------------------------------------
    // Hardware Abstraction Alignments
    // ----------------------------------------------------
    constexpr uint32 GPU_PIPELINE_FLAG_OPAQUE = 1U << 0U;
    constexpr uint32 GPU_PIPELINE_FLAG_TRANSPARENT = 1U << 1U;
    constexpr uint32 GPU_PIPELINE_FLAG_SHADOW = 1U << 2U;

    // Number of vec4 slots of generic scalar/vector parameter scratch in a material record.
    inline constexpr uint32 GPU_MATERIAL_PARAM_VEC4_COUNT = 8U;
    // Number of texture binding slots per material; each holds a bindless globalTextures[] index.
    inline constexpr uint32 GPU_MATERIAL_TEXTURE_SLOT_COUNT = 16U;

    // ----------------------------------------------------
    // Geometry & Layout Structs
    // ----------------------------------------------------
    // Mirrors the GLSL `VertexData` struct in ShaderBase.glsl exactly (std430, 5 x vec4 = 80
    // bytes). Unpacked for now; octahedral/half packing is a future optimization that must change
    // both sides together.
    struct alignas(16) GpuVertexData
    {
        Vec4 position; // xyz used
        Vec4 normal; // xyz used
        Vec4 tangent; // xyzw (w = handedness)
        Vec4 uv; // xy used
        Vec4 color; // rgba
    };

    struct alignas(16) GpuMeshData
    {
        uint32 first_index;
        uint32 index_count;
        int32 base_vertex;
        uint32 vertex_count;
    };

    struct alignas(16) GpuInstanceData
    {
        Mat4 model_matrix;
        Mat4 prev_model_matrix; // (Required for Motion Vectors / TAA / Velocity Pass)
        Vec4 bounds_min;
        Vec4 bounds_max;
        uint32 mesh_id;
        uint32 material_id;

        uint32 padding0;
        uint32 padding1; // Pad end of layout to guarantee uniform safety bounds
    };

    // Mirrors the GLSL `LightData` struct in ShaderBase.glsl exactly (std430, 5 x vec4). Fields are
    // packed into vec4 lanes; accessor comments give the lane meaning.
    struct alignas(16) GpuLightData
    {
        Vec4 position_range; // xyz = world position, w = range
        Vec4 direction_type; // xyz = direction, w = light type (TBX_SHADER_LIGHT_TYPE_*)
        Vec4 color_intensity; // rgb = color, w = intensity
        Vec4 spot_angles_area; // x = inner cos/angle, y = outer, zw = area size
        // x = directional cascade flag (>=0 = this is the directional caster, else -1).
        // y = local shadow atlas base layer (-1 = this local light casts no shadow).
        // z = local shadow view count (1 = spot/area, 6 = point cube faces).
        // w = local shadow far plane (light range) for depth-bias scaling.
        Vec4 shadow_data;
    };

    /// @brief
    /// Purpose: One packed material record stored in the global materials[] SSBO, indexed by
    /// material_id. Generic by design so any material type packs into it without a bespoke struct.
    /// @details
    /// params: scalar/vector parameters flattened by the shader's name->offset contract.
    /// texture_indices: per-slot index into the bindless globalTextures[] handle table.
    /// texture_present: 1 if the slot is bound, else 0 (shader samples a fallback).
    /// Ownership: Copied by value into the materials SSBO. Thread Safety: immutable once uploaded.
    struct alignas(16) GpuMaterialData
    {
        std::array<Vec4, GPU_MATERIAL_PARAM_VEC4_COUNT> params;
        std::array<uint32, GPU_MATERIAL_TEXTURE_SLOT_COUNT> texture_indices;
        std::array<uint32, GPU_MATERIAL_TEXTURE_SLOT_COUNT> texture_present;
        uint32 flags;
        uint32 padding0;
        uint32 padding1;
        uint32 padding2;
    };

    struct alignas(16) GpuClusterGridData
    {
        uint32 light_offset;
        uint32 light_count;

        uint32 padding0;
        uint32 padding1; // Pad end of layout to guarantee uniform safety bounds
    };

    // Mirrors the GLSL `TbxSceneUniforms` std140 UBO in ShaderBase.glsl exactly. The G-buffer is
    // sampled through bound sampler units (not bindless indices), so no per-target indices live
    // here. Motion-vector reprojection (prev_view_projection) is deferred until TAA is in scope.
    struct alignas(16) GpuUniforms
    {
        Mat4 view_projection;
        Mat4 inverse_view_projection;
        // Directional shadow cascades: world -> light-clip per cascade (cascade 0 = nearest/sharpest,
        // the last = furthest/lowest-res). Each cascade's colored transmittance map is rendered and
        // sampled with this same per-cascade matrix, so no separate full-range color matrix is needed.
        std::array<Mat4, SHADOW_CASCADE_COUNT> cascade_view_projection;

        std::array<Vec4, 6U> frustum_planes;

        Vec4 ambient_light;
        Vec4 camera_position_time; // xyz = camera position, w = elapsed time
        Vec4 shadow_settings; // x = slope bias, y = constant bias, z = PCF radius (texels)
        Vec4 cascade_splits; // x..w = furthest camera distance covered by cascade 0..3
        Vec4 sky_color;
        Vec4 sky_params;
        Vec4 screen_size; // xy = size in pixels, zw = inverse size
        Vec4 cluster_dimensions; // xyz = cluster grid dims, w = depth slice scale

        uint32 current_pass_filter;
        uint32 total_instance_count;
        uint32 total_mesh_count;
        uint32 total_material_count;
        uint32 light_count;
        uint32 shadow_count; // 1 if a directional caster is active this frame, else 0
        uint32 cascade_count; // active directional shadow cascades (<= SHADOW_CASCADE_COUNT)
        uint32 max_scene_draw_count;
        uint32 max_shadow_draw_count;
    };

    // Mirrors the GLSL `TbxShadowPassUniforms` std140 UBO. One per directional cascade caster
    // sub-pass: the world -> light-clip matrix the depth/color caster shaders transform vertices by.
    struct alignas(16) GpuShadowPassUniforms
    {
        Mat4 view_projection;
    };

    // Mirrors the GLSL `TbxPostUniforms` std140 UBO in Post.glsl. One per post-processing effect:
    // it names the effect's packed material record (in the shared materials[] table) so the
    // fullscreen shader reads its params/textures, plus the effect's stack blend weight.
    struct alignas(16) GpuPostUniforms
    {
        uint32 post_material_id;
        float blend;
        uint32 padding0;
        uint32 padding1;
    };

    // Native API Mapping Layout for GPU Indirect Execution
    struct GpuIndexedDrawCommand
    {
        uint32 index_count;
        uint32 instance_count;
        uint32 first_index;
        int32 base_vertex;
        uint32 first_instance;
    };
}
