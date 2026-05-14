#version 450 core

layout(location = 0) out vec4 o_color;

in vec2 v_tex_coord;

layout(std140, binding = 1) uniform ToyboxMaterialBlock
{
    vec4 u_material_uniforms[64];
};

layout(binding = 0) uniform sampler2D u_scene_color;
layout(binding = 1) uniform sampler2D u_lut;

void main()
{
    vec4 tint = u_material_uniforms[0];
    vec4 emissive = u_material_uniforms[1];
    float strength = max(u_material_uniforms[2].x, 0.0);
    float blend = clamp(u_material_uniforms[3].x, 0.0, 1.0);
    vec4 source = texture(u_scene_color, v_tex_coord);
    vec3 color = clamp(source.rgb, 0.0, 1.0);

    // Dimensions for 1024x32
    float size = 32.0;
    float width = 1024.0;
    float height = 32.0;

    // 1. Blue channel selects the slice
    float blue = color.b * (size - 1.0);
    float b0 = floor(blue);
    float b1 = ceil(blue);

    // 2. Constants for half-texel offsets and scaling
    float x_offset = 0.5 / width;
    float y_offset = 0.5 / height;
    float x_scale = (size - 1.0) / width;
    float y_scale = (size - 1.0) / height;

    // 3. Compute Vertical Coordinate (INVERTED)
    // 1.0 - (offset + scaled_green) flips the Y-axis
    float lut_y = 1.0 - (y_offset + (color.g * y_scale));

    // 4. Compute Horizontal UVs
    vec2 uv0 = vec2((b0 / size) + x_offset + (color.r * x_scale), lut_y);
    vec2 uv1 = vec2((b1 / size) + x_offset + (color.r * x_scale), lut_y);

    // 5. Sample and Lerp
    vec3 graded0 = textureLod(u_lut, uv0, 0.0).rgb;
    vec3 graded1 = textureLod(u_lut, uv1, 0.0).rgb;

    vec3 final_color = mix(graded0, graded1, fract(blue));
    vec3 graded = mix(source.rgb, final_color, strength * blend);
    graded *= tint.rgb;
    o_color = vec4(graded + emissive.rgb, source.a * tint.a);
}
