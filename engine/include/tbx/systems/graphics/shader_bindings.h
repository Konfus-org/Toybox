#pragma once
#include "tbx/types/material.h"
#include "tbx/types/matrices.h"
#include "tbx/types/typedefs.h"
#include <array>
#include <optional>
#include <string_view>

namespace tbx
{
    // IMPORTANT: KEEP THESE BINDINGS AND SHADER STRUCTS IN SYNC WITH SHADERS!

    constexpr uint32 BINDING_FRAME_DATA = 0U;
    constexpr uint32 BINDING_CAMERA_DATA = 1U;
    constexpr uint32 BINDING_OBJECT_DATA = 2U;
    constexpr uint32 BINDING_MATERIAL_DATA = 3U;

    constexpr uint32 BINDING_ALBEDO_MAP = 10U;
    constexpr uint32 BINDING_NORMAL_MAP = 11U;
    constexpr uint32 BINDING_METALLIC_ROUGHNESS_MAP = 12U;
    constexpr uint32 BINDING_AO_MAP = 13U;
    constexpr uint32 BINDING_EMISSIVE_MAP = 14U;
    constexpr uint32 BINDING_SKYBOX_TEXTURE = 15U;
    constexpr uint32 BINDING_SECONDARY_SKYBOX_TEXTURE = 16U;

    constexpr uint32 BINDING_LIGHT_DATA = 20U;
    constexpr uint32 BINDING_SHADOW_MAP = 30U;
    constexpr uint32 BINDING_SHADOW_MASK = 31U;
    constexpr uint32 BINDING_SHADOW_PASS_DATA = 32U;

    constexpr uint32 BINDING_GBUFFER_ALBEDO = 50U;
    constexpr uint32 BINDING_GBUFFER_NORMAL = 51U;
    constexpr uint32 BINDING_GBUFFER_MATERIAL = 52U;
    constexpr uint32 BINDING_GBUFFER_EMISSIVE = 53U;
    constexpr uint32 BINDING_GBUFFER_DEPTH = 54U;

    constexpr uint32 BINDING_POST_SOURCE_COLOR = 60U;
    constexpr uint32 BINDING_POST_SOURCE_DEPTH = 61U;

    constexpr uint32 PARAM_ALBEDO_MAP = make_param_id("albedo_map");
    constexpr uint32 PARAM_NORMAL_MAP = make_param_id("normal_map");
    constexpr uint32 PARAM_METALLIC_ROUGHNESS_MAP = make_param_id("metallic_roughness_map");
    constexpr uint32 PARAM_AO_MAP = make_param_id("ao_map");
    constexpr uint32 PARAM_EMISSIVE_MAP = make_param_id("emissive_map");
    constexpr uint32 PARAM_SKYBOX_TEXTURE = make_param_id("skybox_texture");
    constexpr uint32 PARAM_SECONDARY_SKYBOX_TEXTURE = make_param_id("secondary_skybox_texture");
    constexpr uint32 PARAM_GBUFFER_ALBEDO = make_param_id("gbuffer_albedo");
    constexpr uint32 PARAM_GBUFFER_NORMAL = make_param_id("gbuffer_normal");
    constexpr uint32 PARAM_GBUFFER_MATERIAL = make_param_id("gbuffer_material");
    constexpr uint32 PARAM_GBUFFER_EMISSIVE = make_param_id("gbuffer_emissive");
    constexpr uint32 PARAM_GBUFFER_DEPTH = make_param_id("gbuffer_depth");
    constexpr uint32 PARAM_SHADOW_MASK = make_param_id("shadow_mask");
    constexpr uint32 PARAM_SOURCE_COLOR = make_param_id("source_color");
    constexpr uint32 PARAM_SOURCE_DEPTH = make_param_id("source_depth");

    constexpr uint32 VERTEX_BUFFER_SLOT_MESH = 0U;
    constexpr uint32 VERTEX_BUFFER_SLOT_INSTANCE = 1U;

    constexpr uint32 VERTEX_ATTRIBUTE_POSITION = 0U;
    constexpr uint32 VERTEX_ATTRIBUTE_NORMAL = 1U;
    constexpr uint32 VERTEX_ATTRIBUTE_TANGENT = 2U;
    constexpr uint32 VERTEX_ATTRIBUTE_TEX_COORD = 3U;
    constexpr uint32 VERTEX_ATTRIBUTE_COLOR = 4U;
    constexpr uint32 VERTEX_ATTRIBUTE_INSTANCE_MODEL = 5U;
    constexpr uint32 VERTEX_ATTRIBUTE_INSTANCE_NORMAL = 9U;

    constexpr uint32 MAX_LIGHTS = 128U;
    constexpr uint32 DIRECTIONAL_SHADOW_CASCADE_COUNT = 4U;
    constexpr float SHADER_LIGHT_TYPE_DIRECTIONAL = 0.0F;
    constexpr float SHADER_LIGHT_TYPE_POINT = 1.0F;
    constexpr float SHADER_LIGHT_TYPE_SPOT = 2.0F;

    struct alignas(16) FrameShaderData
    {
        float time = 0.0F;
        float delta_time = 0.0F;
        Vec2 viewport_size = Vec2(1.0F, 1.0F);
    };

    struct alignas(16) CameraShaderData
    {
        Mat4 view = Mat4(1.0F);
        Mat4 projection = Mat4(1.0F);
        Mat4 view_projection = Mat4(1.0F);
        Mat4 inverse_view = Mat4(1.0F);
        Mat4 inverse_projection = Mat4(1.0F);
        Vec4 world_position = Vec4(0.0F, 0.0F, 0.0F, 1.0F);
    };

    // TODO: rename to ModelShaderData
    struct alignas(16) ObjectShaderData
    {
        Mat4 model = Mat4(1.0F);
        Mat4 normal_matrix = Mat4(1.0F); // TODO: rename to normal
    };

    struct alignas(16) ShaderLightData
    {
        Vec4 position_type = Vec4(0.0F);
        Vec4 direction_range = Vec4(0.0F);
        Vec4 color_intensity = Vec4(0.0F);
        Vec4 params = Vec4(0.0F);
    };

    struct alignas(16) LightingShaderData
    {
        Vec4 ambient_color = Vec4(0.2F, 0.2F, 0.2F, 1.0F);
        IVec4 light_meta = IVec4(0, 0, 0, 0);
        Vec4 light_padding = Vec4(0.0F);
        std::array<ShaderLightData, MAX_LIGHTS> lights = {};
    };

    struct alignas(16) ShadowShaderData
    {
        std::array<Mat4, MAX_LIGHTS> light_view_projections = {};
        std::array<Vec4, MAX_LIGHTS> light_directions = {};
        std::array<Vec4, MAX_LIGHTS> shadow_params = {};
        std::array<Vec4, MAX_LIGHTS> shadow_extra_params = {};
        IVec4 shadow_meta = IVec4(0, 0, 0, 0);
    };

    inline std::optional<uint32> resolve_shader_texture_slot(uint32 binding_id);

    inline std::optional<uint32> resolve_shader_texture_slot(const std::string_view binding_name)
    {
        return resolve_shader_texture_slot(make_param_id(binding_name));
    }

    inline std::optional<uint32> resolve_shader_texture_slot(const uint32 binding_id)
    {
        if (binding_id == PARAM_ALBEDO_MAP)
            return BINDING_ALBEDO_MAP;

        if (binding_id == PARAM_NORMAL_MAP)
            return BINDING_NORMAL_MAP;

        if (binding_id == PARAM_METALLIC_ROUGHNESS_MAP)
            return BINDING_METALLIC_ROUGHNESS_MAP;

        if (binding_id == PARAM_AO_MAP)
            return BINDING_AO_MAP;

        if (binding_id == PARAM_EMISSIVE_MAP)
            return BINDING_EMISSIVE_MAP;

        if (binding_id == PARAM_SKYBOX_TEXTURE)
            return BINDING_SKYBOX_TEXTURE;

        if (binding_id == PARAM_SECONDARY_SKYBOX_TEXTURE)
            return BINDING_SECONDARY_SKYBOX_TEXTURE;

        if (binding_id == PARAM_GBUFFER_ALBEDO)
            return BINDING_GBUFFER_ALBEDO;

        if (binding_id == PARAM_GBUFFER_NORMAL)
            return BINDING_GBUFFER_NORMAL;

        if (binding_id == PARAM_GBUFFER_MATERIAL)
            return BINDING_GBUFFER_MATERIAL;

        if (binding_id == PARAM_GBUFFER_EMISSIVE)
            return BINDING_GBUFFER_EMISSIVE;

        if (binding_id == PARAM_GBUFFER_DEPTH)
            return BINDING_GBUFFER_DEPTH;

        if (binding_id == PARAM_SHADOW_MASK)
            return BINDING_SHADOW_MASK;

        if (binding_id == PARAM_SOURCE_COLOR)
            return BINDING_POST_SOURCE_COLOR;

        if (binding_id == PARAM_SOURCE_DEPTH)
            return BINDING_POST_SOURCE_DEPTH;

        return std::nullopt;
    }
}
