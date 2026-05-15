#version 450 core
#include Globals.glsl

layout(location = 0) out vec4 o_final_color;
layout(location = 1) out vec4 o_geometry_preview_color;
layout(location = 2) out vec4 o_albedo;
layout(location = 3) out vec4 o_normal;
layout(location = 4) out vec4 o_depth_preview;
layout(location = 5) out vec4 o_emissive;
layout(location = 6) out vec4 o_material;

in vec3 v_local_direction;

layout(binding = 0) uniform sampler2D u_textures[1];

const float TBX_PI = 3.14159265359;

void main()
{
    vec3 direction = normalize(v_local_direction);
    float longitude = atan(direction.z, direction.x);
    float latitude = atan(-direction.y, length(direction.xz));
    vec2 uv = vec2(
        longitude / (2.0 * TBX_PI) + 0.5,
        0.5 - latitude / TBX_PI
    );
    
    // At the poles longitude becomes degenerate — a tiny spatial step spans the
    // full U range, so the GPU's auto-LOD picks an incorrect (blurry) mip level.
    // Override with explicit derivatives, clamping the longitude component to
    // the physical angular density at this latitude (cos(lat) shrinks to 0 at poles).
    vec2 duvdx = dFdx(uv);
    vec2 duvdy = dFdy(uv);
    float max_du = cos(latitude) * 0.5 + 0.001;
    duvdx.x = clamp(duvdx.x, -max_du, max_du);
    duvdy.x = clamp(duvdy.x, -max_du, max_du);
    vec4 color = u_material_uniforms[0];
    vec4 emissive_color = u_material_uniforms[1];
    float exposure = max(u_material_uniforms[4].x, 0.0);
    float color_texture_blend = clamp(u_material_uniforms[5].x, 0.0, 1.0);
    vec4 texture_color = textureGrad(u_textures[0], uv, duvdx, duvdy);
    vec4 base_color = mix(color, color * texture_color, color_texture_blend);
    vec3 emissive = emissive_color.rgb * exposure;
    vec4 final_color = vec4(clamp(base_color.rgb + emissive, 0.0, 1.0), 1.0);

    o_final_color = final_color;
    o_geometry_preview_color = final_color;
    o_albedo = final_color;
    o_normal = vec4(0.5, 0.5, 1.0, 1.0);
    o_depth_preview = vec4(0.0, 0.0, 0.0, 1.0);
    o_emissive = vec4(emissive, 1.0);
    o_material = vec4(0.0, 0.0, 0.0, 1.0);
}
