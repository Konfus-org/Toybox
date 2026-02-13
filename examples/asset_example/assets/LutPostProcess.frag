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

    vec2 lut_resolution = vec2(textureSize(u_lut, 0));
    float size = lut_resolution.y;
    vec2 texel_size = 1.0 / lut_resolution;

    // Determine the blue slices
    float blue = color.b * (size - 1.0);
    float b0 = floor(blue);
    float b1 = ceil(blue);

    // Scale and offset R and G to sample from the center of texels
    // This prevents "bleeding" between color tiles
    vec2 offset = 0.5 * texel_size;
    vec2 scale = vec2((size - 1.0) * texel_size.x, (size - 1.0) * texel_size.y);
    float lut_y = 1.0 - (offset.y + color.g * scale.y);

    // Map RG to the first blue slice (b0)
    vec2 uv0 = vec2((b0 / size) + offset.x + (color.r * scale.x), lut_y);
    // Map RG to the second blue slice (b1)
    vec2 uv1 = vec2((b1 / size) + offset.x + (color.r * scale.x), lut_y);

    vec3 graded0 = textureLod(u_lut, uv0, 0.0).rgb;
    vec3 graded1 = textureLod(u_lut, uv1, 0.0).rgb;

    vec3 final_color = mix(graded0, graded1, fract(blue));
    float blend = clamp(u_strength * u_blend, 0.0, 1.0);
    o_color = vec4(mix(source.rgb, final_color, blend), source.a);
}
