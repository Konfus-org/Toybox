#include "ShaderBase.glsl"

// Transparent glass surface. Shares the forward+ PBR body shading but adds the two cues that make a
// pane read as glass instead of a flat tinted hole:
//   1. a Fresnel-weighted environment reflection (the rim catches the sky), and
//   2. Fresnel-driven opacity, so grazing angles turn reflective/solid while the face stays clear.
// Pairs with Pbr.vert. Parameter lanes (positional float stream, declared .mat order):
//   params[0] = albedo_color (rgba; a = base see-through opacity)
//   params[1] = emissive_color (rgba)
//   params[2] = (metallic, roughness, normal_strength, ao)
// Texture slots (declared .mat order): 0 albedo, 1 normal, 2 metallic, 3 roughness, 4 ao, 5 emissive.
layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;
layout(location = 2) in vec3 v_world_position;
layout(location = 3) in vec3 v_world_normal;
layout(location = 4) in vec4 v_world_tangent;
layout(location = 5) in flat uint v_material_id;

layout(location = 0) out vec4 o_color;

// Cheap image-based-lighting stand-in: there is no reflection probe, so approximate the environment
// from the scene uniforms — reflections pointing up pick up the authored sky tint, those pointing
// down fade to a dimmed ambient "ground" so the underside still reads as reflective glass.
vec3 tbx_env_reflection(vec3 direction)
{
    float up = tbx_saturate(direction.y * 0.5 + 0.5);
    vec3 ground = ambientLight.rgb * 0.5;
    return mix(ground, skyColor.rgb, up);
}

void main()
{
    uint mid = v_material_id;
    vec4 albedo_color = tbx_material_param(mid, 0u);
    vec4 emissive_color = tbx_material_param(mid, 1u);
    vec4 scalars = tbx_material_param(mid, 2u);
    float normal_strength = scalars.z;

    vec4 base = v_color * albedo_color * tbx_sample_material_texture(mid, 0u, v_uv, vec4(1.0));

    vec3 view_direction = normalize(cameraPositionTime.xyz - v_world_position);
    vec3 normal = normalize(v_world_normal);
    // Two-sided pane: present whichever face points at the viewer so the reflection and lighting are
    // computed against a forward-facing normal.
    if (dot(normal, view_direction) < 0.0)
        normal = -normal;
    if (tbx_material_has_texture(mid, 1u))
    {
        vec3 tangent = normalize(v_world_tangent.xyz);
        vec3 bitangent = normalize(cross(normal, tangent) * v_world_tangent.w);
        vec3 sampled = tbx_sample_material_texture(mid, 1u, v_uv, vec4(0.5, 0.5, 1.0, 1.0)).xyz * 2.0 - 1.0;
        sampled.xy *= normal_strength;
        normal = normalize(mat3(tangent, bitangent, normal) * sampled);
    }

    float metallic = clamp(scalars.x * tbx_sample_material_texture(mid, 2u, v_uv, vec4(1.0)).r, 0.0, 1.0);
    float roughness = scalars.y * tbx_sample_material_texture(mid, 3u, v_uv, vec4(1.0)).r;
    float ao = clamp(scalars.w * tbx_sample_material_texture(mid, 4u, v_uv, vec4(1.0)).r, 0.0, 1.0);
    vec3 emissive = emissive_color.rgb * tbx_sample_material_texture(mid, 5u, v_uv, vec4(1.0)).rgb;

    // Lit body: the transmitted tint plus the direct-light specular glints (tight at low roughness).
    vec3 lit = tbx_shade_pbr(base.rgb, metallic, roughness, ao, normal, v_world_position, view_direction);
    vec3 body = tbx_tonemap(lit + emissive);

    // Schlick Fresnel about glass' ~0.04 base reflectance. A small floor keeps a faint sky sheen even
    // head-on so the pane never collapses back into a pure hole.
    float ndotv = tbx_saturate(dot(normal, view_direction));
    float fresnel = mix(0.1, 1.0, pow(1.0 - ndotv, 5.0));

    // The sky/environment reflection is already in display range (matches the sky pass, which is not
    // tonemapped), so mix it against the tonemapped body rather than re-tonemapping it.
    vec3 reflection = tbx_env_reflection(reflect(-view_direction, normal));
    vec3 color = mix(body, reflection, fresnel);

    // Opacity rises with Fresnel: the face stays see-through, the rim turns into solid reflection.
    float alpha = clamp(base.a + fresnel * (1.0 - base.a), 0.0, 1.0);

    o_color = vec4(color, alpha);
}
