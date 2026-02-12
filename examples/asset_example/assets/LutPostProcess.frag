#version 450 core

layout(location = 0) out vec4 o_color;

in vec2 v_tex_coord;

uniform sampler2D u_scene_color;
uniform sampler2D u_lut;
uniform float u_strength = 1.0;
uniform float u_blend = 1.0;

void main()
{
    vec4 source = texture(u_scene_color, v_tex_coord);
    vec3 color = clamp(source.rgb, 0.0, 1.0);

    float size = 16.0;

    // Determine the blue slices
    float blue = color.b * (size - 1.0);
    float b0 = floor(blue);
    float b1 = ceil(blue);

    // Scale and offset R and G to sample from the center of texels
    // This prevents "bleeding" between color tiles
    vec2 offset = vec2(0.5 / 256.0, 0.5 / 16.0);
    vec2 scale = vec2((size - 1.0) / 256.0, (size - 1.0) / 16.0);

    // Map RG to the first blue slice (b0)
    vec2 uv0 = vec2(b0 / size, 0.0) + offset + (color.rg * scale);
    // Map RG to the second blue slice (b1)
    vec2 uv1 = vec2(b1 / size, 0.0) + offset + (color.rg * scale);

    vec3 graded0 = texture(u_lut, uv0).rgb;
    vec3 graded1 = texture(u_lut, uv1).rgb;

    vec3 final_color = mix(graded0, graded1, fract(blue));
    o_color = vec4(mix(source.rgb, final_color, u_strength), source.a);
}
