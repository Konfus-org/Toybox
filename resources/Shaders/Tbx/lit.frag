#version 460 core
// Toybox builtin lit fragment stage: Blinn-Phong with 3x3 PCF soft shadows.
in VertexData
{
    vec3 world_position;
    vec3 world_normal;
    vec4 shadow_coords;
    vec2 uv;
} vertex;

uniform vec4 u_tint;
uniform vec4 u_light_color;
uniform float u_light_intensity;
uniform vec3 u_light_direction;
uniform vec3 u_camera_position;
uniform sampler2D u_shadow_map;
uniform sampler2D u_albedo;

out vec4 out_color;

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

void main()
{
    const vec3 normal = normalize(vertex.world_normal);
    const vec3 to_light = -u_light_direction;
    const float lambert = max(dot(normal, to_light), 0.0);
    const float shadowing = shadow_factor(vertex.shadow_coords, lambert);

    const vec3 to_camera = normalize(u_camera_position - vertex.world_position);
    const vec3 half_vector = normalize(to_light + to_camera);
    const float specular = pow(max(dot(normal, half_vector), 0.0), 32.0) * 0.35;

    const float ambient = 0.25;
    const float direct = (lambert + specular) * shadowing * u_light_intensity;
    const vec4 albedo = texture(u_albedo, vertex.uv) * u_tint;
    out_color = vec4(albedo.rgb * u_light_color.rgb * (ambient + direct), albedo.a);
}
