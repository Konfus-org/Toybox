#version 460 core
// Toybox builtin PBR fragment stage: metallic-roughness Cook-Torrance (GGX distribution,
// Smith geometry, Schlick fresnel) with 3x3 PCF soft shadows and derivative-based normal
// mapping (no vertex tangents required).
in VertexData
{
    vec3 world_position;
    vec3 world_normal;
    vec4 shadow_coords;
    vec2 uv;
} vertex;

uniform vec4 u_tint; // material albedo color x toy tint
uniform vec4 u_light_color;
uniform float u_light_intensity;
uniform vec3 u_light_direction;
uniform vec3 u_camera_position;
uniform sampler2D u_shadow_map;
uniform sampler2D u_albedo;
uniform sampler2D u_normal_map;
uniform sampler2D u_metallic_roughness_map;
uniform float u_metallic;
uniform float u_roughness;
uniform vec4 u_emissive;
uniform int u_has_normal_map;
uniform int u_has_metallic_roughness_map;
uniform float u_uv_scale;

out vec4 out_color;

const float PI = 3.14159265;

float shadow_factor(vec4 shadow_coords, float lambert)
{
    const vec3 projected = shadow_coords.xyz / shadow_coords.w * 0.5 + 0.5;
    if (projected.z > 1.0)
        return 1.0;

    // Slope-scaled bias keeps grazing surfaces from self-shadowing.
    const float bias = max(0.0025 * (1.0 - lambert), 0.0005);
    const vec2 texel = 1.0 / vec2(textureSize(u_shadow_map, 0));
    float lit = 0.0;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
        {
            const float closest = texture(u_shadow_map, projected.xy + vec2(x, y) * texel).r;
            lit += projected.z - bias > closest ? 0.0 : 1.0;
        }
    return lit / 9.0;
}

vec3 surface_normal(vec2 uv)
{
    const vec3 normal = normalize(vertex.world_normal);
    if (u_has_normal_map == 0)
        return normal;

    // Cotangent frame from screen-space derivatives — no vertex tangents required.
    const vec3 dp1 = dFdx(vertex.world_position);
    const vec3 dp2 = dFdy(vertex.world_position);
    const vec2 duv1 = dFdx(uv);
    const vec2 duv2 = dFdy(uv);
    const vec3 dp2perp = cross(dp2, normal);
    const vec3 dp1perp = cross(normal, dp1);
    const vec3 tangent = dp2perp * duv1.x + dp1perp * duv2.x;
    const vec3 bitangent = dp2perp * duv1.y + dp1perp * duv2.y;
    const float inverse_scale =
        inversesqrt(max(dot(tangent, tangent), dot(bitangent, bitangent)) + 0.0001);
    const mat3 frame = mat3(tangent * inverse_scale, bitangent * inverse_scale, normal);
    const vec3 sampled = texture(u_normal_map, uv).xyz * 2.0 - 1.0;
    return normalize(frame * sampled);
}

void main()
{
    const vec2 uv = vertex.uv * u_uv_scale;
    const vec4 albedo_sample = texture(u_albedo, uv) * u_tint;
    const vec3 albedo = albedo_sample.rgb;

    float metallic = u_metallic;
    float roughness = u_roughness;
    if (u_has_metallic_roughness_map == 1)
    {
        const vec3 sampled = texture(u_metallic_roughness_map, uv).rgb;
        roughness *= sampled.g;
        metallic *= sampled.b;
    }
    roughness = clamp(roughness, 0.045, 1.0);
    metallic = clamp(metallic, 0.0, 1.0);

    const vec3 normal = surface_normal(uv);
    const vec3 to_camera = normalize(u_camera_position - vertex.world_position);
    const vec3 to_light = -u_light_direction;
    const vec3 half_vector = normalize(to_light + to_camera);
    const float n_dot_l = max(dot(normal, to_light), 0.0);
    const float n_dot_v = max(dot(normal, to_camera), 0.0001);
    const float n_dot_h = max(dot(normal, half_vector), 0.0);
    const float v_dot_h = max(dot(to_camera, half_vector), 0.0);

    // Cook-Torrance specular: GGX distribution, Smith geometry, Schlick fresnel.
    const float alpha = roughness * roughness;
    const float alpha2 = alpha * alpha;
    const float distribution_denominator = n_dot_h * n_dot_h * (alpha2 - 1.0) + 1.0;
    const float distribution =
        alpha2 / (PI * distribution_denominator * distribution_denominator);
    const float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    const float geometry = (n_dot_v / (n_dot_v * (1.0 - k) + k))
        * (n_dot_l / (n_dot_l * (1.0 - k) + k));
    const vec3 f0 = mix(vec3(0.04), albedo, metallic);
    const vec3 fresnel = f0 + (1.0 - f0) * pow(1.0 - v_dot_h, 5.0);
    const vec3 specular =
        distribution * geometry * fresnel / max(4.0 * n_dot_v * n_dot_l, 0.0001);

    // The light's intensity is illuminance on a fully lit diffuse surface (1/PI folded in),
    // keeping authored intensities comparable to simple lambert lighting.
    const vec3 diffuse = (1.0 - fresnel) * (1.0 - metallic) * albedo;
    const float shadowing = shadow_factor(vertex.shadow_coords, n_dot_l);
    const vec3 radiance = u_light_color.rgb * u_light_intensity;
    const vec3 direct = (diffuse + specular) * radiance * n_dot_l * shadowing;
    const vec3 ambient = albedo * 0.25 * (1.0 - metallic * 0.6);
    out_color = vec4(direct + ambient + u_emissive.rgb, albedo_sample.a);
}
