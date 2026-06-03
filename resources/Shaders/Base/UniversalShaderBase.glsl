#version 460 core
#extension GL_EXT_nonuniform_qualifier : enable

#define TBX_SHADER_BINDING_GLOBAL_ENTITIES 0
#define TBX_SHADER_BINDING_GLOBAL_MATERIALS 1
#define TBX_SHADER_BINDING_DRAW_COMMAND_LOOKUP 2
#define TBX_SHADER_BINDING_INDIRECT_COMMANDS 3
#define TBX_SHADER_BINDING_VISIBLE_ENTITY_IDS 4
#define TBX_SHADER_BINDING_SCENE_UNIFORMS 5
#define TBX_SHADER_BINDING_GLOBAL_TEXTURES 6

#define TBX_SHADER_MATERIAL_LOOKUP_STRIDE 1024u
#define TBX_SHADER_PIPELINE_FLAG_OPAQUE (1u << 0u)
#define TBX_SHADER_PIPELINE_FLAG_TRANSPARENT (1u << 1u)
#define TBX_SHADER_PIPELINE_FLAG_SHADOW (1u << 2u)

#define TBX_BINDING_GBUFFER_FINAL_COLOR 55
#define TBX_BINDING_POST_EFFECT_TEXTURE0 56
#define TBX_MAX_GLOBAL_TEXTURES 32
#define TBX_INVALID_DRAW_SLOT 0xffffffffu
#define TBX_EPSILON 0.00001
#define TBX_PI 3.14159265358979323846
#define TBX_INV_PI 0.31830988618379067154
#define TBX_INV_TAU 0.15915494309189533577

layout(binding = TBX_SHADER_BINDING_GLOBAL_TEXTURES) uniform sampler2D globalTextures[TBX_MAX_GLOBAL_TEXTURES];

float tbx_saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

vec2 tbx_saturate(vec2 value)
{
    return clamp(value, vec2(0.0), vec2(1.0));
}

vec3 tbx_saturate(vec3 value)
{
    return clamp(value, vec3(0.0), vec3(1.0));
}

vec4 tbx_saturate(vec4 value)
{
    return clamp(value, vec4(0.0), vec4(1.0));
}

vec4 tbx_write_to_final_color(vec3 color, float alpha)
{
    return vec4(color, alpha);
}

vec4 tbx_sample_global_texture(uint texture_index, vec2 tex_coord)
{
    // Material uploads reserve the last slot as a valid fallback, so bad indices stay defined.
    uint resolved_texture_index = min(texture_index, uint(TBX_MAX_GLOBAL_TEXTURES - 1));
#ifdef GL_EXT_nonuniform_qualifier
    return texture(globalTextures[nonuniformEXT(resolved_texture_index)], tex_coord);
#else
    return texture(globalTextures[resolved_texture_index], tex_coord);
#endif
}
