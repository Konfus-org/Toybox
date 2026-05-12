#version 450 core
#include Globals.glsl

layout(location = 0) out vec4 o_final_color;
layout(location = 1) out vec4 o_geometry_preview_color;
layout(location = 2) out vec4 o_albedo;
layout(location = 3) out vec4 o_normal;
layout(location = 4) out vec4 o_depth_preview;
layout(location = 5) out vec4 o_emissive;
layout(location = 6) out vec4 o_material;

in vec4 v_color;
in vec2 v_tex_coord;
in vec3 v_world_position;
in vec3 v_world_normal;

layout(binding = 0) uniform sampler2D u_textures[1];

void main()
{
    vec4 texture_color = v_color;
    texture_color *= texture(u_textures[0], v_tex_coord);

    vec4 emissive_color = u_material_uniforms[1];
    float alpha_cutoff = clamp(u_material_uniforms[2].x, 0.0, 1.0);
    if (texture_color.a < alpha_cutoff)
        discard;

    float surface_alpha = texture_color.a * (1.0 - clamp(u_material_uniforms[3].x, 0.0, 1.0));
    vec3 normalized_world_normal = normalize(v_world_normal);
    vec3 emissive = emissive_color.rgb * max(u_material_uniforms[4].x, 0.0);
    vec3 preview_color = clamp(texture_color.rgb + emissive, 0.0, 1.0);
    float depth_preview = 1.0 - pow(clamp(gl_FragCoord.z, 0.0, 1.0), 24.0);

    o_final_color = vec4(preview_color, surface_alpha);
    o_geometry_preview_color = vec4(preview_color, surface_alpha);
    o_albedo = vec4(texture_color.rgb, surface_alpha);
    o_normal = vec4((normalized_world_normal * 0.5) + 0.5, surface_alpha);
    o_depth_preview = vec4(vec3(depth_preview), 1.0);
    o_emissive = vec4(emissive, 1.0);
    o_material = vec4(0.0, 1.0, 0.0, 1.0);
}
