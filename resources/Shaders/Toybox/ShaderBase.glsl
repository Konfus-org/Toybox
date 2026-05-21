#ifndef TBX_SHADER_BASE_GLSL
#define TBX_SHADER_BASE_GLSL

#define TBX_PI 3.14159265359
#define TBX_EPSILON 0.00001

#define TBX_BINDING_FRAME_DATA      0
#define TBX_BINDING_CAMERA_DATA     1
#define TBX_BINDING_OBJECT_DATA     2
#define TBX_BINDING_MATERIAL_DATA   3

#define TBX_BINDING_ALBEDO_MAP              10
#define TBX_BINDING_NORMAL_MAP              11
#define TBX_BINDING_METALLIC_ROUGHNESS_MAP  12
#define TBX_BINDING_AO_MAP                   13
#define TBX_BINDING_EMISSIVE_MAP             14
#define TBX_BINDING_SKYBOX_TEXTURE           15
#define TBX_BINDING_SECONDARY_SKYBOX_TEXTURE 16

#define TBX_BINDING_LIGHT_DATA       20
#define TBX_BINDING_SHADOW_MAP       30
#define TBX_BINDING_SHADOW_MASK      31
#define TBX_BINDING_SHADOW_PASS_DATA 32

#define TBX_BINDING_GBUFFER_ALBEDO    50
#define TBX_BINDING_GBUFFER_NORMAL    51
#define TBX_BINDING_GBUFFER_MATERIAL  52
#define TBX_BINDING_GBUFFER_EMISSIVE  53
#define TBX_BINDING_GBUFFER_DEPTH     54

#define TBX_BINDING_POST_SOURCE_COLOR 60
#define TBX_BINDING_POST_SOURCE_DEPTH 61

#define TBX_MAX_LIGHTS 128

#define TBX_LIGHT_TYPE_DIRECTIONAL 0.0
#define TBX_LIGHT_TYPE_POINT       1.0
#define TBX_LIGHT_TYPE_SPOT        2.0

layout(std140, binding = TBX_BINDING_FRAME_DATA) uniform TbxFrameData
{
    float u_time;
    float u_delta_time;
    vec2  u_viewport_size;
};

layout(std140, binding = TBX_BINDING_CAMERA_DATA) uniform TbxCameraData
{
    mat4 u_view;
    mat4 u_projection;
    mat4 u_view_projection;
    mat4 u_inverse_view;
    mat4 u_inverse_projection;
    vec4 u_camera_world_position;
};

layout(std140, binding = TBX_BINDING_OBJECT_DATA) uniform TbxObjectData
{
    mat4 u_model;
    mat4 u_normal_matrix;
};

struct TbxLight
{
    // xyz = world position
    // w   = light type
    vec4 position_type;

    // xyz = world direction
    // w   = range/radius
    vec4 direction_range;

    // rgb = color
    // w   = intensity
    vec4 color_intensity;

    // x = inner cone cos
    // y = outer cone cos
    // z = shadow index, or -1 if unshadowed
    // w = shadow layer count
    vec4 params;
};

layout(std140, binding = TBX_BINDING_LIGHT_DATA) uniform TbxLightData
{
    vec4 u_ambient_color;
    int u_light_count;
    vec3 u_light_padding;

    TbxLight u_lights[TBX_MAX_LIGHTS];
};

float saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

vec2 saturate(vec2 value)
{
    return clamp(value, vec2(0.0), vec2(1.0));
}

vec3 saturate(vec3 value)
{
    return clamp(value, vec3(0.0), vec3(1.0));
}

vec4 saturate(vec4 value)
{
    return clamp(value, vec4(0.0), vec4(1.0));
}

vec3 tbx_unpack_normal(vec3 packed_normal)
{
    return normalize(packed_normal * 2.0 - 1.0);
}

float tbx_linearize_depth(float depth, float near_plane, float far_plane)
{
    float z = depth * 2.0 - 1.0;
    return (2.0 * near_plane * far_plane) / (far_plane + near_plane - z * (far_plane - near_plane));
}

vec3 tbx_reconstruct_world_position(vec2 uv, float depth)
{
    vec4 clip_position = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view_position = u_inverse_projection * clip_position;
    view_position /= max(view_position.w, TBX_EPSILON);

    vec4 world_position = u_inverse_view * view_position;
    return world_position.xyz;
}

vec3 tbx_tonemap_aces(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;

    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

vec3 tbx_apply_exposure_tonemap_gamma(vec3 color, float exposure, float gamma)
{
    color *= exposure;
    color = tbx_tonemap_aces(color);
    color = pow(color, vec3(1.0 / max(gamma, TBX_EPSILON)));

    return color;
}

#endif
