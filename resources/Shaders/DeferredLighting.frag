#version 450 core
#include DeferredLighting.glsl
#include DeferredShadows.glsl
#include Globals.glsl

in vec2 v_tex_coord;

layout(location = 0) out vec4 o_color;

uniform sampler2D u_gbuffer_albedo;
uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_emissive;
uniform sampler2D u_gbuffer_material;
uniform sampler2D u_gbuffer_depth;
uniform sampler2DArray u_shadow_map;

layout(std140, binding = 7) uniform LightingInfoBuffer
{
    LightingInfo u_lighting_info;
};

layout(std140, binding = 8) uniform ShadowInfoBuffer
{
    ShadowInfo u_shadow_info;
};

layout(std430, binding = 0) readonly buffer PointLightsBuffer
{
    PointLight u_point_lights[];
};

layout(std430, binding = 1) readonly buffer SpotLightsBuffer
{
    SpotLight u_spot_lights[];
};

layout(std430, binding = 2) readonly buffer TileLightSpansBuffer
{
    TileLightSpan u_tile_light_spans[];
};

layout(std430, binding = 3) readonly buffer TilePointLightIndicesBuffer
{
    uint u_tile_point_light_indices[];
};

layout(std430, binding = 4) readonly buffer TileSpotLightIndicesBuffer
{
    uint u_tile_spot_light_indices[];
};

layout(std430, binding = 5) readonly buffer AreaLightsBuffer
{
    AreaLight u_area_lights[];
};

layout(std430, binding = 6) readonly buffer TileAreaLightIndicesBuffer
{
    uint u_tile_area_light_indices[];
};

void main()
{
    float depth = texture(u_gbuffer_depth, v_tex_coord).r;
    if (depth >= 0.999999)
    {
        o_color = u_lighting_info.clear_color;
        return;
    }

    vec4 albedo_sample = texture(u_gbuffer_albedo, v_tex_coord);
    vec4 normal_sample = texture(u_gbuffer_normal, v_tex_coord);
    vec4 emissive_sample = texture(u_gbuffer_emissive, v_tex_coord);
    vec4 material_sample = texture(u_gbuffer_material, v_tex_coord);

    vec3 albedo = tbx_srgb_to_linear(albedo_sample.rgb);
    vec3 emissive = tbx_srgb_to_linear(emissive_sample.rgb);
    vec3 normal = normalize((normal_sample.xyz * 2.0) - 1.0);
    vec3 world_position =
        tbx_reconstruct_world_position(v_tex_coord, depth, u_lighting_info.inverse_view_projection);
    vec3 view_delta = u_lighting_info.camera_position.xyz - world_position;
    vec3 view_direction = normalize(view_delta + vec3(0.0, 0.0, step(length(view_delta), 0.0001)));
    float view_depth = length(view_delta);

    float specular_strength = clamp(material_sample.r, 0.0, 1.0);
    float shininess = clamp(material_sample.g, 1.0, 256.0);
    float occlusion = clamp(material_sample.b, 0.0, 1.0);
    float exposure = max(emissive_sample.a, 0.0);

    vec3 diffuse_accumulation = vec3(0.0);
    vec3 specular_accumulation = vec3(0.0);
    float hemisphere_factor = clamp((normal.y * 0.5) + 0.5, 0.0, 1.0);
    vec3 hemisphere_ambient =
        mix(vec3(0.05, 0.045, 0.04), vec3(0.17, 0.19, 0.23), hemisphere_factor) * occlusion;
    float fresnel = pow(1.0 - max(dot(normal, view_direction), 0.0), 4.0);
    vec3 ambient_accumulation = albedo * hemisphere_ambient;
    vec3 fresnel_accumulation = vec3(specular_strength * fresnel * 0.08);
    uint tile_x =
        min(uint(gl_FragCoord.x) / uint(max(u_lighting_info.tile_info.x, 1)),
            uint(max(u_lighting_info.tile_info.y - 1, 0)));
    uint tile_y =
        min(uint(gl_FragCoord.y) / uint(max(u_lighting_info.tile_info.x, 1)),
            uint(max(u_lighting_info.tile_info.z - 1, 0)));
    uint tile_index = tile_y * uint(max(u_lighting_info.tile_info.y, 1)) + tile_x;
    TileLightSpan tile_light_span = u_tile_light_spans[tile_index];

    for (int light_index = 0; light_index < u_lighting_info.light_counts.x; ++light_index)
    {
        DirectionalLight light = u_lighting_info.directional_lights[light_index];
        vec3 light_direction = normalize(-light.direction);
        float shadow_visibility =
            mix(1.0,
                tbx_get_shadow_visibility(
                    u_shadow_map,
                    u_shadow_info,
                    normal,
                    light_direction,
                    world_position,
                    view_depth,
                    gl_FragCoord.xy),
                clamp(light.casts_shadows, 0.0, 1.0));
        ambient_accumulation += albedo * light.radiance * (light.ambient_intensity * occlusion);
        tbx_accumulate_light(
            albedo,
            normal,
            view_direction,
            light_direction,
            light.radiance,
            shadow_visibility,
            specular_strength,
            shininess,
            diffuse_accumulation,
            specular_accumulation);
    }

    for (uint tile_light_index = 0U; tile_light_index < tile_light_span.point_count;
         ++tile_light_index)
    {
        PointLight light = u_point_lights
            [u_tile_point_light_indices[tile_light_span.point_offset + tile_light_index]];
        vec3 to_light = light.position - world_position;
        float distance_squared = dot(to_light, to_light);
        vec3 light_direction = to_light * inversesqrt(max(distance_squared, 0.0001));
        float attenuation = tbx_get_distance_attenuation(distance_squared, light.range);
        tbx_accumulate_light(
            albedo,
            normal,
            view_direction,
            light_direction,
            light.radiance,
            attenuation,
            specular_strength,
            shininess,
            diffuse_accumulation,
            specular_accumulation);
    }

    for (uint tile_light_index = 0U; tile_light_index < tile_light_span.spot_count;
         ++tile_light_index)
    {
        SpotLight light = u_spot_lights
            [u_tile_spot_light_indices[tile_light_span.spot_offset + tile_light_index]];
        vec3 to_light = light.position - world_position;
        float distance_squared = dot(to_light, to_light);
        vec3 light_direction = to_light * inversesqrt(max(distance_squared, 0.0001));
        float attenuation = tbx_get_distance_attenuation(distance_squared, light.range);
        float spot_cos = dot(light.direction, -light_direction);
        float cone = clamp(
            (spot_cos - light.outer_cos) / max(light.inner_cos - light.outer_cos, 0.0001),
            0.0,
            1.0);
        float cone_mask = step(light.outer_cos, spot_cos);
        tbx_accumulate_light(
            albedo,
            normal,
            view_direction,
            light_direction,
            light.radiance,
            attenuation * cone * cone * cone_mask,
            specular_strength,
            shininess,
            diffuse_accumulation,
            specular_accumulation);
    }

    for (uint tile_light_index = 0U; tile_light_index < tile_light_span.area_count;
         ++tile_light_index)
    {
        AreaLight light = u_area_lights
            [u_tile_area_light_indices[tile_light_span.area_offset + tile_light_index]];
        vec3 closest_point = tbx_get_area_light_closest_point(light, world_position);
        vec3 to_light = closest_point - world_position;
        float distance_squared = dot(to_light, to_light);
        vec3 light_direction =
            mix(-light.direction,
                to_light * inversesqrt(max(distance_squared, 0.0001)),
                step(0.0001, distance_squared));
        float attenuation =
            tbx_get_area_light_attenuation(light, light_direction, distance_squared);
        tbx_accumulate_light(
            albedo,
            normal,
            view_direction,
            light_direction,
            light.radiance,
            attenuation,
            specular_strength,
            shininess,
            diffuse_accumulation,
            specular_accumulation);
    }

    vec3 final_color =
        ambient_accumulation + diffuse_accumulation + specular_accumulation + fresnel_accumulation;
    final_color += emissive;
    final_color *= exposure;

    vec3 display_color = tbx_linear_to_srgb(final_color);
    float dither = tbx_interleaved_gradient_noise(gl_FragCoord.xy) - 0.5;
    display_color += vec3(dither / 255.0);
    o_color = vec4(clamp(display_color, 0.0, 1.0), albedo_sample.a);
}
