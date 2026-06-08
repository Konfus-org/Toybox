#pragma once
#include "tbx/types/matrices.h"
#include "tbx/types/typedefs.h"
#include <array>

namespace tbx
{
    // IMPORTANT: KEEP THESE BINDINGS AND SHADER STRUCTS IN SYNC WITH SHADERS!

    // OpenGL exposes separate binding namespaces for SSBOs, UBOs, and textures.
    // Keep each resource class densely packed so the backend stays within common driver limits.
    constexpr uint32 SHADER_BINDING_ALL_INSTANCES = 0U;
    constexpr uint32 SHADER_BINDING_GLOBAL_VERTICES = 1U;
    constexpr uint32 SHADER_BINDING_GLOBAL_MESHES = 2U;
    constexpr uint32 SHADER_BINDING_GLOBAL_MATERIALS = 3U;
    constexpr uint32 SHADER_BINDING_GLOBAL_LIGHTS = 4U;
    constexpr uint32 SHADER_BINDING_CLUSTER_GRID = 6U;
    constexpr uint32 SHADER_BINDING_LIGHT_INDEX_POOL = 7U;
    constexpr uint32 SHADER_BINDING_MAIN_SCENE_DRAW_ARGS = 8U;
    constexpr uint32 SHADER_BINDING_SHADOW_ARGS_POOL = 9U;
    constexpr uint32 SHADER_BINDING_MAIN_SCENE_DRAW_COUNT = 10U;
    constexpr uint32 SHADER_BINDING_SHADOW_DRAW_COUNT = 11U;

    constexpr uint32 SHADER_BINDING_SCENE_UNIFORMS = 0U;

    constexpr uint32 SHADER_BINDING_GLOBAL_TEXTURES = 0U;
    constexpr uint32 SHADER_BINDING_GBUFFER_ALBEDO = 17U;
    constexpr uint32 SHADER_BINDING_GBUFFER_ROUGHNESS = 18U;
    constexpr uint32 SHADER_BINDING_GBUFFER_NORMAL = 19U;
    constexpr uint32 SHADER_BINDING_GBUFFER_METALLIC = 20U;
    constexpr uint32 SHADER_BINDING_SCENE_DEPTH = 21U;
    constexpr uint32 SHADER_BINDING_SHADOW_DEPTH_ATLAS = 22U;
    constexpr uint32 SHADER_BINDING_FINAL_HDR = 23U;
    constexpr uint32 SHADER_BINDING_POST_EFFECT_TEXTURE0 = 24U;
    constexpr uint32 SHADER_BINDING_FINAL_HDR_IMAGE = 0U;

    constexpr uint32 SHADER_PIPELINE_FLAG_OPAQUE = 1U << 0U;
    constexpr uint32 SHADER_PIPELINE_FLAG_TRANSPARENT = 1U << 1U;
    constexpr uint32 SHADER_PIPELINE_FLAG_SHADOW = 1U << 2U;

    constexpr uint32 SHADER_LIGHT_TYPE_DIRECTIONAL = 0U;
    constexpr uint32 SHADER_LIGHT_TYPE_POINT = 1U;
    constexpr uint32 SHADER_LIGHT_TYPE_SPOT = 2U;
    constexpr uint32 SHADER_LIGHT_TYPE_AREA = 3U;

    struct alignas(16) ShaderVertexData
    {
        Vec4 position = Vec4(0.0F);
        Vec4 normal = Vec4(0.0F, 1.0F, 0.0F, 0.0F);
        Vec4 tangent = Vec4(1.0F, 0.0F, 0.0F, 1.0F);
        Vec4 uv = Vec4(0.0F);
        Vec4 color = Vec4(1.0F);
    };

    struct alignas(16) ShaderMeshData
    {
        uint32 first_index = 0U;
        uint32 index_count = 0U;
        int32 base_vertex = 0;
        uint32 vertex_count = 0U;
    };

    struct alignas(16) ShaderInstanceData
    {
        Mat4 model_matrix = Mat4(1.0F);
        Vec4 bounds_min = Vec4(0.0F, 0.0F, 0.0F, 0.0F);
        Vec4 bounds_max = Vec4(0.0F, 0.0F, 0.0F, 0.0F);
        uint32 mesh_id = 0U;
        uint32 material_id = 0U;
        uint32 padding0 = 0U;
        uint32 padding1 = 0U;
    };

    struct alignas(16) ShaderMaterialData
    {
        uint32 pipeline_flags = SHADER_PIPELINE_FLAG_OPAQUE;
        uint32 albedo_texture_index = 0U;
        uint32 normal_texture_index = 0U;
        uint32 metallic_texture_index = 0U;
        uint32 roughness_texture_index = 0U;
        uint32 ao_texture_index = 0U;
        uint32 emissive_texture_index = 0U;
        float alpha_cutoff = 0.0F;
        Vec4 base_color = Vec4(1.0F);
        Vec4 emissive_color = Vec4(0.0F, 0.0F, 0.0F, 1.0F);
        Vec4 surface = Vec4(0.0F, 0.5F, 1.0F, 1.0F);
    };

    struct ShaderDrawIndexedIndirectCommand
    {
        uint32 count = 0U;
        uint32 instance_count = 0U;
        uint32 first_index = 0U;
        int32 base_vertex = 0;
        uint32 base_instance = 0U;
    };

    struct alignas(16) ShaderDrawCount
    {
        uint32 count = 0U;
        uint32 padding0 = 0U;
        uint32 padding1 = 0U;
        uint32 padding2 = 0U;
    };

    struct alignas(16) ShaderLightData
    {
        Vec4 position_range = Vec4(0.0F);
        Vec4 direction_type = Vec4(0.0F, 0.0F, -1.0F, 0.0F);
        Vec4 color_intensity = Vec4(1.0F, 1.0F, 1.0F, 0.0F);
        Vec4 spot_angles_area = Vec4(0.0F);
        Vec4 shadow_data = Vec4(-1.0F, 0.0F, 0.0F, 0.0F);
    };

    struct alignas(16) ShaderClusterGridData
    {
        uint32 light_offset = 0U;
        uint32 light_count = 0U;
        uint32 padding0 = 0U;
        uint32 padding1 = 0U;
    };

    struct alignas(16) ShaderSceneUniforms
    {
        Mat4 view_projection = Mat4(1.0F);
        Mat4 inverse_view_projection = Mat4(1.0F);
        std::array<Vec4, 6U> frustum_planes = {};
        Vec4 ambient_light = Vec4(1.0F);
        Vec4 camera_position_time = Vec4(0.0F);
        Vec4 shadow_settings = Vec4(2048.0F, 90.0F, 1.0F, 96.0F);
        Vec4 sky_color = Vec4(1.0F);
        Vec4 sky_params = Vec4(0.0F);
        Vec4 screen_size = Vec4(1.0F, 1.0F, 1.0F, 1.0F);
        Vec4 cluster_dimensions = Vec4(16.0F, 9.0F, 24.0F, 3456.0F);
        uint32 current_pass_filter = SHADER_PIPELINE_FLAG_OPAQUE;
        uint32 total_instance_count = 0U;
        uint32 total_mesh_count = 0U;
        uint32 total_material_count = 0U;
        uint32 light_count = 0U;
        uint32 shadow_count = 0U;
        uint32 max_scene_draw_count = 0U;
        uint32 max_shadow_draw_count = 0U;
    };
}
