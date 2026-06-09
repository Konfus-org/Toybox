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
    constexpr uint32 GPU_BINDING_UNIFORMS = 0U;

    // ----------------------------------------------------
    // Hardware Abstraction Alignments
    // ----------------------------------------------------
    constexpr uint32 GPU_PIPELINE_FLAG_OPAQUE = 1U << 0U;
    constexpr uint32 GPU_PIPELINE_FLAG_TRANSPARENT = 1U << 1U;
    constexpr uint32 GPU_PIPELINE_FLAG_SHADOW = 1U << 2U;

    // ----------------------------------------------------
    // Geometry & Layout Structs
    // ----------------------------------------------------
    // Mirrors the GLSL `VertexData` struct in ShaderBase.glsl exactly (std430, 5 x vec4 = 80 bytes).
    // Unpacked for now; octahedral/half packing is a future optimization that must change both
    // sides together.
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
        Vec4 shadow_data; // x = shadow map/atlas index (-1 if none)
    };

    // Number of vec4 slots of generic scalar/vector parameter scratch in a material record.
    inline constexpr uint32 GPU_MATERIAL_PARAM_VEC4_COUNT = 8U;
    // Number of texture binding slots per material; each holds a bindless globalTextures[] index.
    inline constexpr uint32 GPU_MATERIAL_TEXTURE_SLOT_COUNT = 16U;

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

        std::array<Vec4, 6U> frustum_planes;

        Vec4 ambient_light;
        Vec4 camera_position_time; // xyz = camera position, w = elapsed time
        Vec4 shadow_settings;
        Vec4 sky_color;
        Vec4 sky_params;
        Vec4 screen_size; // xy = size in pixels, zw = inverse size
        Vec4 cluster_dimensions; // xyz = cluster grid dims, w = depth slice scale

        uint32 current_pass_filter;
        uint32 total_instance_count;
        uint32 total_mesh_count;
        uint32 total_material_count;
        uint32 light_count;
        uint32 shadow_count;
        uint32 max_scene_draw_count;
        uint32 max_shadow_draw_count;
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
