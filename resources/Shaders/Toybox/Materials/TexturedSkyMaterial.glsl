#include "Toybox/ShaderBase.glsl"

layout(std140, binding = TBX_BINDING_MATERIAL_DATA) uniform TbxTexturedSkyMaterialData
{
    vec4 u_color;

    float u_brightness;
    vec3 _tbx_pad_u_brightness;
    float u_ambient_multiplier;
    vec3 _tbx_pad_u_ambient_multiplier;
    float u_blend_factor;
    vec3 _tbx_pad_u_blend_factor;
};

layout(binding = TBX_BINDING_SKYBOX_TEXTURE) uniform sampler2D u_skybox_texture;
layout(binding = TBX_BINDING_SECONDARY_SKYBOX_TEXTURE) uniform sampler2D u_secondary_skybox_texture;

vec2 tbx_sky_direction_to_equirectangular_uv(vec3 direction)
{
    vec3 normalized_direction = normalize(direction);
    float longitude = atan(normalized_direction.z, normalized_direction.x);
    float latitude = asin(clamp(normalized_direction.y, -1.0, 1.0));

    return vec2((longitude / (2.0 * TBX_PI)) + 0.5, (latitude / TBX_PI) + 0.5);
}

vec4 tbx_sample_textured_sky_color(vec3 direction)
{
    vec2 uv = tbx_sky_direction_to_equirectangular_uv(direction);

    vec4 primary = texture(u_skybox_texture, uv);
    vec4 secondary = texture(u_secondary_skybox_texture, uv);
    vec4 sky = mix(primary, secondary, saturate(u_blend_factor));

    float ambient_luminance = dot(u_ambient_color.rgb, vec3(0.2126, 0.7152, 0.0722));
    float ambient_factor = mix(1.0, ambient_luminance, saturate(u_ambient_multiplier));
    sky.rgb *= u_color.rgb * u_brightness * ambient_factor;
    sky.a *= u_color.a;
    return sky;
}
