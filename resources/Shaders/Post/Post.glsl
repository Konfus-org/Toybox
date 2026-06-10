#include "ShaderBase.glsl"

// ---------------------------------------------------------------------------------------------
// Post-processing API.
//
// A post-processing effect is an ordinary material drawn as a single fullscreen triangle. It reads
// the previous stage's color through tbx_post_input(), and reads its OWN parameters and textures
// from the shared material table addressed by tbxPostMaterialId — exactly like a surface material,
// so any effect is authored with the normal material/shader system and needs no bespoke plumbing.
//
// To author an effect: write a fragment shader that `#include "Post.glsl"`, reference
// `Shaders/Post/Fullscreen.vert` as its vertex stage in the .mat, declare params/textures in the
// .mat, and write o_color (usually `tbx_post_apply(uv, effect_rgb)`).
// ---------------------------------------------------------------------------------------------

#define TBX_SHADER_BINDING_POST_UNIFORMS 5

// Mirrors GpuPostUniforms (std140). One per effect: names the effect's packed material record and
// carries the effect's stack blend weight.
layout(std140, binding = TBX_SHADER_BINDING_POST_UNIFORMS) uniform TbxPostUniforms
{
    uint tbxPostMaterialId;
    float tbxPostBlend;
    uint tbxPostPad0;
    uint tbxPostPad1;
};

// Previous stage color: the rendered scene for the first effect, the prior effect's output after.
vec4 tbx_post_input(vec2 uv)
{
    return texture(tbx_scene_color, uv);
}

// This effect's material parameters (positional float-stream lanes) and textures (declared slots).
vec4 tbx_post_param(uint lane)
{
    return tbx_material_param(tbxPostMaterialId, lane);
}

vec4 tbx_post_texture(uint slot, vec2 uv, vec4 fallback)
{
    return tbx_sample_material_texture(tbxPostMaterialId, slot, uv, fallback);
}

bool tbx_post_has_texture(uint slot)
{
    return tbx_material_has_texture(tbxPostMaterialId, slot);
}

// The resident sampler behind one of this effect's texture slots, for textureSize/textureLod use
// (e.g. LUT lookups). The caller must check tbx_post_has_texture(slot) first.
sampler2D tbx_post_sampler(uint slot)
{
    return globalTextures[materials[tbxPostMaterialId].textureIndices[slot]];
}

// The effect's stack blend weight (0 = source unchanged, 1 = full effect) and the screen texel size.
float tbx_post_blend()
{
    return tbxPostBlend;
}

vec2 tbx_post_texel()
{
    return screenSize.zw;
}

// Mixes an effect result with the source by the effect's blend weight — the common output helper.
vec3 tbx_post_apply(vec2 uv, vec3 effect_color)
{
    return mix(tbx_post_input(uv).rgb, effect_color, tbx_saturate(tbxPostBlend));
}
