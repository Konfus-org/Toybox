#pragma once
#include "tbx/types/matrices.h"

namespace tbx
{
    // IMPORTANT: KEEP THESE BINDINGS AND SHADER STRUCTS IN SYNC WITH SHADERS!

    constexpr uint32 SHADER_BINDING_GLOBAL_ENTITIES = 0U;
    constexpr uint32 SHADER_BINDING_GLOBAL_MATERIALS = 1U;
    constexpr uint32 SHADER_BINDING_DRAW_COMMAND_LOOKUP = 2U;
    constexpr uint32 SHADER_BINDING_INDIRECT_COMMANDS = 3U;
    constexpr uint32 SHADER_BINDING_VISIBLE_ENTITY_IDS = 4U;
    constexpr uint32 SHADER_BINDING_SCENE_UNIFORMS = 5U;
    constexpr uint32 SHADER_BINDING_GLOBAL_TEXTURES = 6U;
    constexpr uint32 SHADER_MATERIAL_LOOKUP_STRIDE = 1024U;

    constexpr uint32 SHADER_PIPELINE_FLAG_OPAQUE = 1U << 0U;
    constexpr uint32 SHADER_PIPELINE_FLAG_TRANSPARENT = 1U << 1U;
    constexpr uint32 SHADER_PIPELINE_FLAG_SHADOW = 1U << 2U;

    constexpr uint32 VERTEX_BUFFER_SLOT_MESH = 0U;
    constexpr uint32 VERTEX_BUFFER_SLOT_INSTANCE = 1U;

    constexpr uint32 VERTEX_ATTRIBUTE_POSITION = 0U;
    constexpr uint32 VERTEX_ATTRIBUTE_NORMAL = 1U;
    constexpr uint32 VERTEX_ATTRIBUTE_TANGENT = 2U;
    constexpr uint32 VERTEX_ATTRIBUTE_TEX_COORD = 3U;
    constexpr uint32 VERTEX_ATTRIBUTE_COLOR = 4U;
    constexpr uint32 VERTEX_ATTRIBUTE_INSTANCE_MODEL = 5U;
    constexpr uint32 VERTEX_ATTRIBUTE_INSTANCE_NORMAL = 9U;

    struct alignas(16) ShaderEntityData
    {
        Mat4 model_matrix = Mat4(1.0F);
        Vec4 bounding_sphere = Vec4(0.0F, 0.0F, 0.0F, 1.0F);
        uint32 mesh_id = 0U;
        uint32 material_id = 0U;
        uint32 padding0 = 0U;
        uint32 padding1 = 0U;
    };

    struct alignas(16) ShaderMaterialData
    {
        uint32 pipeline_flags = SHADER_PIPELINE_FLAG_OPAQUE;
        uint32 texture_index = 0U;
        uint32 padding0 = 0U;
        uint32 padding1 = 0U;
        Vec4 base_color = Vec4(1.0F);
    };

    struct ShaderDrawIndexedIndirectCommand
    {
        uint32 count = 0U;
        uint32 instance_count = 0U;
        uint32 first_index = 0U;
        uint32 base_vertex = 0U;
        uint32 base_instance = 0U;
    };

    struct alignas(16) ShaderSceneUniforms
    {
        Mat4 view_projection = Mat4(1.0F);
        std::array<Vec4, 6U> frustum_planes = {};
        uint32 current_pass_filter = SHADER_PIPELINE_FLAG_OPAQUE;
        uint32 total_entity_count = 0U;
        uint32 padding0 = 0U;
        uint32 padding1 = 0U;
    };
}
