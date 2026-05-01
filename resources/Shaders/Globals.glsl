layout(std140, binding = 0) uniform ToyboxViewBlock
{
    mat4 u_view_proj;
};

layout(std140, binding = 1) uniform ToyboxMaterialBlock
{
    vec4 u_color;
    vec4 u_emissive;
    float u_specular_strength;
    float u_shininess_strength;
    float u_alpha_cutoff;
    float u_material_surface_padding0;
    float u_transparency_amount;
    float u_exposure;
    float u_diffuse_strength;
    float u_normal_strength;
    float u_emissive_strength;
    float u_color_texture_blend;
    float u_wireframe_width;
    float u_material_surface_padding1;
    float u_material_surface_padding2;
    float u_material_surface_padding3;
};

mat4 tbx_get_model_matrix(const mat4 model)
{
    return model;
}

uint tbx_get_instance_id(const uint instance_id)
{
    return instance_id;
}

vec3 tbx_srgb_to_linear(vec3 color)
{
    return pow(max(color, vec3(0.0)), vec3(2.2));
}

vec3 tbx_linear_to_srgb(vec3 color)
{
    return pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));
}

vec3 tbx_tonemap_aces(vec3 color)
{
    // ACES fitted tonemap (Narkowicz 2015)
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

float tbx_interleaved_gradient_noise(vec2 pixel_coord)
{
    return fract(52.9829189 * fract(dot(pixel_coord, vec2(0.06711056, 0.00583715))));
}
