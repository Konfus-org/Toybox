#version 450 core

layout(location = 0) in vec3 a_position;

layout(location = 0) out flat uint v_packed_light_index;

uniform mat4 u_view_projection = mat4(1.0);
uniform int u_instance_offset = 0;
uniform int u_light_type = 0;

struct PackedLight
{
    vec4 position_range;
    vec4 direction_inner_cos;
    vec4 color_intensity;
    vec4 area_outer_ambient;
    ivec4 metadata;
};

layout(std430, binding = 0) readonly buffer PackedLightBuffer
{
    PackedLight packed_lights[];
};

layout(std430, binding = 4) readonly buffer LocalLightIndexBuffer
{
    uint local_light_indices[];
};

vec3 safe_normalize(vec3 value, vec3 fallback)
{
    float len2 = dot(value, value);
    if (len2 <= 1e-8)
        return fallback;
    return value * inversesqrt(len2);
}

void build_basis(vec3 axis, out vec3 tangent, out vec3 bitangent)
{
    vec3 fallback_up = abs(axis.y) > 0.95 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    tangent = safe_normalize(cross(fallback_up, axis), vec3(1.0, 0.0, 0.0));
    bitangent = safe_normalize(cross(axis, tangent), vec3(0.0, 0.0, 1.0));
}

void main()
{
    uint instance_index = uint(max(u_instance_offset, 0)) + uint(gl_InstanceID);
    v_packed_light_index = local_light_indices[instance_index];

    PackedLight light = packed_lights[v_packed_light_index];
    vec3 light_position = light.position_range.xyz;
    float light_range = max(light.position_range.w, 0.001);
    vec3 world_position = light_position;

    if (u_light_type == 1)
    {
        world_position = light_position + (a_position * (light_range * 2.0));
    }
    else if (u_light_type == 2)
    {
        float outer_cos = clamp(light.area_outer_ambient.z, -0.9999, 0.9999);
        float outer_angle = acos(outer_cos);
        float cone_radius = tan(outer_angle) * light_range;
        // Packed spot direction points from the light toward illuminated geometry.
        vec3 cone_axis = safe_normalize(light.direction_inner_cos.xyz, vec3(0.0, -1.0, 0.0));
        vec3 tangent = vec3(1.0, 0.0, 0.0);
        vec3 bitangent = vec3(0.0, 0.0, 1.0);
        build_basis(cone_axis, tangent, bitangent);

        float axis_distance = (1.0 - a_position.y) * light_range;
        vec3 radial_offset = (tangent * a_position.x + bitangent * a_position.z) * cone_radius;
        world_position = light_position + (cone_axis * axis_distance) + radial_offset;
    }
    else
    {
        vec3 area_axis = safe_normalize(light.direction_inner_cos.xyz, vec3(0.0, -1.0, 0.0));
        vec3 tangent = vec3(1.0, 0.0, 0.0);
        vec3 bitangent = vec3(0.0, 0.0, 1.0);
        build_basis(area_axis, tangent, bitangent);

        vec2 area_size = max(light.area_outer_ambient.xy, vec2(0.001));
        float axis_distance = (a_position.y + 0.5) * light_range;
        world_position = light_position + (tangent * (a_position.x * area_size.x))
            + (area_axis * axis_distance)
            + (bitangent * (a_position.z * area_size.y));
    }

    gl_Position = u_view_projection * vec4(world_position, 1.0);
}
