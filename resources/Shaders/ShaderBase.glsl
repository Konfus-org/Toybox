#version 460 core
#extension GL_ARB_shader_viewport_layer_array : enable
#extension GL_ARB_bindless_texture : require

// Buffer/texture binding slots. KEEP IN SYNC with GPU_BINDING_* in shader_bindings.h.
#define TBX_SHADER_BINDING_ALL_INSTANCES 0
#define TBX_SHADER_BINDING_GLOBAL_VERTICES 1
#define TBX_SHADER_BINDING_GLOBAL_MATERIALS 3
#define TBX_SHADER_BINDING_GLOBAL_LIGHTS 4
#define TBX_SHADER_BINDING_GLOBAL_TEXTURES 11
#define TBX_SHADER_BINDING_SCENE_UNIFORMS 0
// Directional shadow cascade depth maps occupy consecutive sampler units starting here.
#define TBX_SHADER_CASCADE_COUNT 4
// Fraction of each cascade's distance range over which it cross-fades into the next (larger) cascade
// so the resolution/bias step is a seamless gradient instead of a hard ring.
#define TBX_CASCADE_BLEND_FRACTION 0.2
#define TBX_SHADER_BINDING_SHADOW_CASCADE_0 12
#define TBX_SHADER_BINDING_SHADOW_CASCADE_1 13
#define TBX_SHADER_BINDING_SHADOW_CASCADE_2 14
#define TBX_SHADER_BINDING_SHADOW_CASCADE_3 15
// One colored transmittance map per cascade, on consecutive units after the depth cascades.
#define TBX_SHADER_BINDING_SHADOW_COLOR_0 16
#define TBX_SHADER_BINDING_SHADOW_COLOR_1 17
#define TBX_SHADER_BINDING_SHADOW_COLOR_2 18
#define TBX_SHADER_BINDING_SHADOW_COLOR_3 19
// Local (point/spot/area) light shadow depth atlas: a sampler2DArray whose layers are the per-view
// depth maps (one per spot/area light, six per point light). KEEP IN SYNC with shader_bindings.h.
#define TBX_SHADER_BINDING_LOCAL_SHADOW_ATLAS 20
// Per-view world -> light-clip matrices for the local shadow atlas, indexed by layer (SSBO).
#define TBX_SHADER_BINDING_LOCAL_SHADOW_MATRICES 2
#define TBX_SHADER_BINDING_SHADOW_PASS 6
#define TBX_SHADER_BINDING_FINAL_HDR 23
#define TBX_SHADER_BINDING_TAG_MASK 24

#define TBX_SHADER_LIGHT_TYPE_DIRECTIONAL 0u
#define TBX_SHADER_LIGHT_TYPE_POINT 1u
#define TBX_SHADER_LIGHT_TYPE_SPOT 2u
#define TBX_SHADER_LIGHT_TYPE_AREA 3u

#define TBX_EPSILON 0.00001
#define TBX_PI 3.14159265358979323846
#define TBX_INV_PI 0.31830988618379067154
#define TBX_INV_TAU 0.15915494309189533577

// Shadow sampling tuning (all in shadow-map texels).
// NORMAL_OFFSET: how far to push the sample point along the surface normal before comparing. This
// texel-proportional offset is what actually prevents shadow acne — it scales with a cascade's texel
// size (coarse far cascades get a bigger offset automatically), so the depth bias can stay tiny and
// shadows still hug their casters instead of peter-panning / leaking light under walls.
#define TBX_SHADOW_NORMAL_OFFSET 2.5
// GAP_TEXELS: threshold for closing sub-pixel seams between abutting casters so sunlight can't bleed
// through tiny gaps in the geometry (e.g. the joint between a glass pane and its frame). Used two ways:
// the opaque PCF pinches shut thin lit slits between occluders, and the colored transmittance is
// dilated so a pane's tint covers the seam instead of leaving a white (lit) sliver.
#define TBX_SHADOW_GAP_TEXELS 2.5

// Mirrors GpuVertexData (std430, 5 x vec4).
struct VertexData
{
    vec4 position;
    vec4 normal;
    vec4 tangent;
    vec4 uv;
    vec4 color;
};

// Mirrors GpuInstanceData.
struct InstanceData
{
    mat4 modelMatrix;
    mat4 prevModelMatrix;
    vec4 boundsMin;
    vec4 boundsMax;
    uint meshId;
    uint materialId;
    uint padding0;
    uint padding1;
};

// Mirrors GpuLightData (packed vec4 lanes).
struct LightData
{
    vec4 positionRange; // xyz = position, w = range
    vec4 directionType; // xyz = direction, w = light type
    vec4 colorIntensity; // rgb = color, w = intensity
    vec4 spotAnglesArea; // x = inner angle, y = outer angle (degrees)
    vec4 shadowData; // reserved (shadows are a future pass)
};

// Mirrors GpuMaterialData: a generic packed record. params is the positional float stream from the
// .mat parameters in declared order; textures are addressed by their declared slot.
struct MaterialData
{
    vec4 params[8];
    uint textureIndices[16];
    uint texturePresent[16];
    uint materialFlags;
    uint materialPad0;
    uint materialPad1;
    uint materialPad2;
};

// Bindless texture table: each entry is a resident sampler2D handle, indexed by material slot.
layout(std430, binding = TBX_SHADER_BINDING_GLOBAL_TEXTURES) readonly buffer TbxGlobalTextureTable
{
    sampler2D globalTextures[];
};

// HDR scene color, sampled by the tonemap blit.
layout(binding = TBX_SHADER_BINDING_FINAL_HDR) uniform sampler2D tbx_scene_color;
// Silhouette mask of the entities a tag-gated post effect gates on (white where tagged). Bound for
// every post effect; a gated effect (e.g. an outline) edge-detects it. Empty/black when nothing is tagged.
layout(binding = TBX_SHADER_BINDING_TAG_MASK) uniform sampler2D tbx_tag_mask;

// Directional shadow cascades (depth, each in its own light clip space). Cascade 0 is the nearest,
// highest-resolution slice; the furthest cascade is the lowest resolution and reaches the configured
// shadow_render_distance. The forward pass picks one per fragment by camera distance. Only sampled
// when a light's shadowData.x >= 0, which the CPU sets on the frame's directional caster.
layout(binding = TBX_SHADER_BINDING_SHADOW_CASCADE_0) uniform sampler2D tbx_shadow_cascade_0;
layout(binding = TBX_SHADER_BINDING_SHADOW_CASCADE_1) uniform sampler2D tbx_shadow_cascade_1;
layout(binding = TBX_SHADER_BINDING_SHADOW_CASCADE_2) uniform sampler2D tbx_shadow_cascade_2;
layout(binding = TBX_SHADER_BINDING_SHADOW_CASCADE_3) uniform sampler2D tbx_shadow_cascade_3;

// Directional translucent shadow maps (RGB transmittance), one per cascade in that cascade's own light
// clip space. Transparent casters multiply their tint into them; the forward pass multiplies the
// directional light by the selected cascade's value so colored glass casts a tinted, partial shadow at
// every distance. Each is cleared to white (1 = transmissive).
layout(binding = TBX_SHADER_BINDING_SHADOW_COLOR_0) uniform sampler2D tbx_shadow_color_0;
layout(binding = TBX_SHADER_BINDING_SHADOW_COLOR_1) uniform sampler2D tbx_shadow_color_1;
layout(binding = TBX_SHADER_BINDING_SHADOW_COLOR_2) uniform sampler2D tbx_shadow_color_2;
layout(binding = TBX_SHADER_BINDING_SHADOW_COLOR_3) uniform sampler2D tbx_shadow_color_3;

// Local (point/spot/area) light shadow depth atlas. Each layer is one light view's depth map in that
// view's own light clip space (a spot/area light owns one layer; a point light owns six cube faces).
// A fragment selects its light's layer via the light's shadowData and depth-compares so local lights
// stop bleeding through walls. Unlike the named cascade samplers, an array layer CAN be dynamically
// indexed, so one sampler serves every local light.
layout(binding = TBX_SHADER_BINDING_LOCAL_SHADOW_ATLAS) uniform sampler2DArray tbx_local_shadow_atlas;

// Mirrors GpuUniforms (std140) exactly — keep every field for ABI parity even if a given shader
// only reads a subset.
layout(std140, binding = TBX_SHADER_BINDING_SCENE_UNIFORMS) uniform TbxSceneUniforms
{
    mat4 viewProjection;
    mat4 inverseViewProjection;
    mat4 cascadeViewProjection[TBX_SHADER_CASCADE_COUNT];
    vec4 frustumPlanes[6];
    vec4 ambientLight;
    vec4 cameraPositionTime; // xyz = camera position, w = elapsed time
    vec4 shadowSettings; // x = slope bias, y = constant bias, z = PCF radius (texels)
    vec4 cascadeSplits; // x..w = furthest camera distance covered by cascade 0..3
    vec4 skyColor;
    vec4 skyParams;
    vec4 screenSize; // xy = pixels, zw = inverse size
    vec4 clusterDimensions;
    uint currentPassFilter;
    uint totalInstanceCount;
    uint totalMeshCount;
    uint totalMaterialCount;
    uint lightCount;
    uint shadowCount;
    uint cascadeCount;
    uint maxSceneDrawCount;
    uint maxShadowDrawCount;
};

// Per-cascade caster matrix, bound only during the directional shadow caster sub-passes. The depth
// and color caster shaders transform vertices by this (one cascade at a time).
layout(std140, binding = TBX_SHADER_BINDING_SHADOW_PASS) uniform TbxShadowPassUniforms
{
    mat4 shadowPassViewProjection;
};

layout(std430, binding = TBX_SHADER_BINDING_ALL_INSTANCES) readonly buffer TbxAllInstancesBuffer
{
    InstanceData instances[];
};

layout(std430, binding = TBX_SHADER_BINDING_GLOBAL_VERTICES) readonly buffer TbxGlobalVertexBuffer
{
    VertexData vertices[];
};

layout(std430, binding = TBX_SHADER_BINDING_GLOBAL_MATERIALS) readonly buffer TbxGlobalMaterialBuffer
{
    MaterialData materials[];
};

layout(std430, binding = TBX_SHADER_BINDING_GLOBAL_LIGHTS) readonly buffer TbxGlobalLightBuffer
{
    LightData lights[];
};

// Per-view world -> light-clip matrices for the local shadow atlas; layer index == array index. A
// spot/area light contributes one matrix, a point light six (its cube faces). Empty/unused when no
// local light casts a shadow this frame (the buffer still carries one identity element so it binds).
layout(std430, binding = TBX_SHADER_BINDING_LOCAL_SHADOW_MATRICES) readonly buffer TbxLocalShadowMatrices
{
    mat4 localShadowMatrices[];
};

float tbx_saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

vec3 tbx_saturate(vec3 value)
{
    return clamp(value, vec3(0.0), vec3(1.0));
}

vec4 tbx_sample_global_texture(uint texture_index, vec2 tex_coord)
{
    return texture(globalTextures[texture_index], tex_coord);
}

// ACES filmic tonemap (Narkowicz 2015 fit) + gamma encode so lit surfaces present at a normal
// brightness when written straight to the (linear, 8-bit) swapchain. Filmic rolloff keeps more
// contrast and saturation than a plain Reinhard curve, which reads as flat/washed out. Richer
// tonemapping/grading belongs to a later user-controlled post-processing stage.
vec3 tbx_tonemap(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    color = (color * (a * color + b)) / (color * (c * color + d) + e);
    return pow(tbx_saturate(color), vec3(1.0 / 2.2));
}

// ---------------------------------------------------------------------------------------------
// Material accessors (data-driven): parameters are a positional float stream in params[]; textures
// are addressed by their declared slot through the bindless globalTextures[] table.
// ---------------------------------------------------------------------------------------------
vec4 tbx_material_param(uint material_id, uint lane)
{
    return materials[material_id].params[lane];
}

bool tbx_material_has_texture(uint material_id, uint slot)
{
    return materials[material_id].texturePresent[slot] != 0u;
}

vec4 tbx_sample_material_texture(uint material_id, uint slot, vec2 uv, vec4 fallback)
{
    if (materials[material_id].texturePresent[slot] == 0u)
        return fallback;
    return texture(globalTextures[materials[material_id].textureIndices[slot]], uv);
}

// Same as tbx_sample_material_texture but with caller-supplied screen-space gradients for the mip
// selection. Use this when the uv is derived through a discontinuous mapping (e.g. an equirectangular
// atan2 longitude) whose implicit dFdx/dFdy would spike at the wrap seam and collapse the sample to
// the coarsest mip — a visible blurry line. The caller unwraps the gradient across the seam first.
vec4 tbx_sample_material_texture_grad(
    uint material_id, uint slot, vec2 uv, vec2 ddx, vec2 ddy, vec4 fallback)
{
    if (materials[material_id].texturePresent[slot] == 0u)
        return fallback;
    return textureGrad(globalTextures[materials[material_id].textureIndices[slot]], uv, ddx, ddy);
}

// ---------------------------------------------------------------------------------------------
// Forward+ PBR. Material fragment shaders fill surface values and call tbx_shade_pbr, which loops
// the (CPU distance-culled) light list. Deferred resolve has been retired so authors shade directly.
// ---------------------------------------------------------------------------------------------
vec3 tbx_light_direction(LightData light, vec3 world_position)
{
    if (uint(light.directionType.w) == TBX_SHADER_LIGHT_TYPE_DIRECTIONAL)
        return normalize(-light.directionType.xyz);
    return normalize(light.positionRange.xyz - world_position);
}

// Believable distance falloff: an inverse-square core multiplied by a smooth window that drives it to
// exactly zero at the light's range (no hard pop at the cull boundary). The inverse-square term is
// range-normalized — 1/(1 + k*(d/range)^2) — so the beam is bright near, ~half by a quarter of the
// range and faint by two-thirds (the natural light-drop a flashlight has), while authored intensities
// stay in an intuitive O(1..10) range regardless of how big the range is (unlike raw 1/d^2, which
// forces wildly different intensities for a near fill vs. a long throw).
float tbx_distance_attenuation(float dist2, float range)
{
    float x2 = dist2 / max(range * range, TBX_EPSILON); // (d / range)^2
    float window = tbx_saturate(1.0 - x2);
    float inverse_square = 1.0 / (1.0 + 6.0 * x2);
    return inverse_square * window * window;
}

float tbx_light_attenuation(LightData light, vec3 world_position)
{
    if (uint(light.directionType.w) == TBX_SHADER_LIGHT_TYPE_DIRECTIONAL)
        return 1.0;
    vec3 to_light = light.positionRange.xyz - world_position;
    return tbx_distance_attenuation(dot(to_light, to_light), light.positionRange.w);
}

float tbx_spotlight_factor(LightData light, vec3 world_position)
{
    if (uint(light.directionType.w) != TBX_SHADER_LIGHT_TYPE_SPOT)
        return 1.0;
    vec3 to_fragment = normalize(world_position - light.positionRange.xyz);
    float cos_theta = dot(normalize(light.directionType.xyz), to_fragment);
    float inner_cos = cos(radians(light.spotAnglesArea.x));
    float outer_cos = cos(radians(light.spotAnglesArea.y));
    // smoothstep gives a soft, perceptually even penumbra between the inner (full) and outer (zero)
    // cone instead of the abrupt edge a linear/quadratic ramp leaves.
    return smoothstep(outer_cos, inner_cos, cos_theta);
}

float tbx_distribution_ggx(vec3 n, vec3 h, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float ndoth = max(dot(n, h), 0.0);
    float denom = (ndoth * ndoth * (a2 - 1.0)) + 1.0;
    return a2 / max(TBX_PI * denom * denom, TBX_EPSILON);
}

float tbx_geometry_schlick(float ndotv, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) * 0.125;
    return ndotv / max((ndotv * (1.0 - k)) + k, TBX_EPSILON);
}

float tbx_geometry_smith(vec3 n, vec3 v, vec3 l, float roughness)
{
    return tbx_geometry_schlick(max(dot(n, v), 0.0), roughness)
           * tbx_geometry_schlick(max(dot(n, l), 0.0), roughness);
}

vec3 tbx_fresnel_schlick(float cos_theta, vec3 f0)
{
    return f0 + (vec3(1.0) - f0) * pow(1.0 - clamp(cos_theta, 0.0, 1.0), 5.0);
}

// Fresnel with a roughness-aware ceiling, used for the ambient (environment) term so rough
// surfaces don't get an unrealistically bright rim from the constant ambient irradiance.
vec3 tbx_fresnel_schlick_roughness(float cos_theta, vec3 f0, float roughness)
{
    vec3 f90 = max(vec3(1.0 - roughness), f0);
    return f0 + (f90 - f0) * pow(1.0 - clamp(cos_theta, 0.0, 1.0), 5.0);
}

// Specular occlusion (Lagarde/Frostbite approximation): stops concave/AO-darkened areas from
// showing a full-strength ambient specular highlight that a plain AO multiply would miss.
float tbx_specular_occlusion(float ndotv, float ao, float roughness)
{
    return tbx_saturate(pow(ndotv + ao, exp2(-16.0 * roughness - 1.0)) - 1.0 + ao);
}

// 3x3 PCF over `shadow_map` at `proj` (light NDC remapped to [0,1]), comparing proj.z - bias against
// stored depth. The sample step is shadowSettings.z texels (shadow_softness), so larger softens edges.
float tbx_shadow_pcf(sampler2D shadow_map, vec3 proj, float bias)
{
    float current = proj.z - bias;
    vec2 unit = 1.0 / vec2(textureSize(shadow_map, 0));
    vec2 texel = unit * max(shadowSettings.z, 1.0);
    float sum = 0.0;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
        {
            float closest = texture(shadow_map, proj.xy + (vec2(x, y) * texel)).r;
            sum += current <= closest ? 1.0 : 0.0;
        }
    float visibility = sum * (1.0 / 9.0);

    // Gap seal: if occluders sit on BOTH opposite sides within the gap threshold, this lit fragment is
    // a sub-pixel crack between abutting geometry (not a real opening), so pinch it shut — sun can't
    // leak through. A real shadow border has an occluder on one side only, so it's left untouched (no
    // halo / shadow growth).
    vec2 g = unit * TBX_SHADOW_GAP_TEXELS;
    float xl = current <= texture(shadow_map, proj.xy - vec2(g.x, 0.0)).r ? 0.0 : 1.0;
    float xr = current <= texture(shadow_map, proj.xy + vec2(g.x, 0.0)).r ? 0.0 : 1.0;
    float yu = current <= texture(shadow_map, proj.xy - vec2(0.0, g.y)).r ? 0.0 : 1.0;
    float yd = current <= texture(shadow_map, proj.xy + vec2(0.0, g.y)).r ? 0.0 : 1.0;
    float pinched = max(min(xl, xr), min(yu, yd));
    return min(visibility, 1.0 - pinched);
}

// World size of one texel of a cascade's maps (its depth and color map share a resolution). Combined
// with half_extent it scales the texel-proportional normal offset consistently across cascades.
float tbx_cascade_texel_world(int cascade, float half_extent)
{
    float res;
    if (cascade == 0)
        res = float(textureSize(tbx_shadow_cascade_0, 0).x);
    else if (cascade == 1)
        res = float(textureSize(tbx_shadow_cascade_1, 0).x);
    else if (cascade == 2)
        res = float(textureSize(tbx_shadow_cascade_2, 0).x);
    else
        res = float(textureSize(tbx_shadow_cascade_3, 0).x);
    return 2.0 * half_extent / res;
}

// Opaque occlusion (0 = shadowed, 1 = lit) from one cascade's depth map at an ALREADY normal-offset
// position. The offset (applied by the caller) is what prevents acne; it must be the SAME point the
// colored transmittance is sampled at so the opaque and colored shadows register exactly.
float tbx_cascade_sample(sampler2D shadow_map, mat4 view_proj, vec3 offset_position, float bias)
{
    vec4 light_clip = view_proj * vec4(offset_position, 1.0);
    vec3 proj = (light_clip.xyz / light_clip.w) * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0;
    return tbx_shadow_pcf(shadow_map, proj, bias);
}

// Reads transmittance, dilating the tint over thin white seams: takes the most-tinted (per-channel min)
// of a small plus-kernel so a glass pane's color closes a sub-pixel gap to its frame instead of leaving
// a white (untinted) sliver that reads as a sunlight leak. The dilation radius is the same gap
// threshold the opaque pinch-seal uses, so both close seams of the same size.
vec3 tbx_transmittance_dilated(sampler2D color_map, vec2 uv)
{
    vec2 g = (1.0 / vec2(textureSize(color_map, 0))) * TBX_SHADOW_GAP_TEXELS;
    vec3 m = texture(color_map, uv).rgb;
    m = min(m, texture(color_map, uv + vec2(g.x, 0.0)).rgb);
    m = min(m, texture(color_map, uv - vec2(g.x, 0.0)).rgb);
    m = min(m, texture(color_map, uv + vec2(0.0, g.y)).rgb);
    m = min(m, texture(color_map, uv - vec2(0.0, g.y)).rgb);
    return m;
}

// Colored transmittance of any transparent casters in this cascade, sampled in the cascade's own light
// clip space at the SAME normal-offset position as the opaque sample. Returns white (no tint) outside
// the cascade's box. Only samplers can't be dynamically indexed, so the color map is dispatched with
// an explicit if/else.
vec3 tbx_cascade_transmittance(int cascade, vec3 offset_position)
{
    vec4 clip = cascadeViewProjection[cascade] * vec4(offset_position, 1.0);
    vec3 proj = (clip.xyz / clip.w) * 0.5 + 0.5;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return vec3(1.0);
    if (cascade == 0)
        return tbx_transmittance_dilated(tbx_shadow_color_0, proj.xy);
    else if (cascade == 1)
        return tbx_transmittance_dilated(tbx_shadow_color_1, proj.xy);
    else if (cascade == 2)
        return tbx_transmittance_dilated(tbx_shadow_color_2, proj.xy);
    return tbx_transmittance_dilated(tbx_shadow_color_3, proj.xy);
}

// Colored visibility (vec3, 1 = fully lit) from a single cascade: the opaque occlusion test (0 =
// shadowed, 1 = lit) multiplied by that cascade's transparent-caster transmittance, so colored glass
// shadows are present and sharp in EVERY cascade, not just one. The depth (opaque/frame) and color
// (glass) maps are sampled at the SAME texel-scaled normal-offset point — otherwise the offset shifts
// the frame's shadow relative to the un-offset glass tint and a fully-lit sliver shows between them on
// the sun-ward side (the white outline). Fully lit outside the cascade.
vec3 tbx_cascade_visibility(int cascade, vec3 world_position, vec3 n, float bias)
{
    mat4 view_proj = cascadeViewProjection[cascade];
    float half_extent = cascadeSplits[cascade];
    float texel_world = tbx_cascade_texel_world(cascade, half_extent);
    vec3 offset_position = world_position + n * (texel_world * TBX_SHADOW_NORMAL_OFFSET);

    float opaque;
    if (cascade == 0)
        opaque = tbx_cascade_sample(tbx_shadow_cascade_0, view_proj, offset_position, bias);
    else if (cascade == 1)
        opaque = tbx_cascade_sample(tbx_shadow_cascade_1, view_proj, offset_position, bias);
    else if (cascade == 2)
        opaque = tbx_cascade_sample(tbx_shadow_cascade_2, view_proj, offset_position, bias);
    else
        opaque = tbx_cascade_sample(tbx_shadow_cascade_3, view_proj, offset_position, bias);
    return opaque * tbx_cascade_transmittance(cascade, offset_position);
}

// Directional shadow as a colored visibility factor (vec3, 1 = fully lit). Picks the cascade whose
// split radius still contains the fragment (nearest = sharpest); across a band before that split it
// cross-fades into the next (larger) cascade so the resolution/bias step is seamless. Each cascade
// already carries its own colored transmittance, so the cross-fade blends the tint too and colored
// glass shadows stay continuous across cascade boundaries (no hard color seam). Fragments outside all
// cascades are fully lit.
vec3 tbx_directional_shadow(vec3 world_position, vec3 n, vec3 l)
{
    float view_dist = length(world_position - cameraPositionTime.xyz);
    int count = int(cascadeCount);
    int cascade = count - 1;
    for (int i = 0; i < count; ++i)
        if (view_dist < cascadeSplits[i])
        {
            cascade = i;
            break;
        }

    float bias = max(shadowSettings.x * (1.0 - dot(n, l)), shadowSettings.y);
    vec3 visibility = tbx_cascade_visibility(cascade, world_position, n, bias);

    // Cross-fade into the next cascade across the outer TBX_CASCADE_BLEND_FRACTION of this cascade's
    // range. At the split both sides evaluate the same (next) cascade, so the seam is continuous.
    float split = cascadeSplits[cascade];
    float prev = cascade > 0 ? cascadeSplits[cascade - 1] : 0.0;
    float band = (split - prev) * TBX_CASCADE_BLEND_FRACTION;
    if (cascade + 1 < count && band > 0.0)
    {
        float t = smoothstep(split - band, split, view_dist);
        if (t > 0.0)
        {
            vec3 next_visibility = tbx_cascade_visibility(cascade + 1, world_position, n, bias);
            visibility = mix(visibility, next_visibility, t);
        }
    }

    return visibility;
}

// 3x3 PCF on one layer of the local shadow atlas, comparing proj.z - bias against stored depth. The
// filter step is shadowSettings.z texels (shared shadow_softness) so larger softens local shadows too.
float tbx_local_shadow_pcf(int layer, vec3 proj, float bias)
{
    float current = proj.z - bias;
    vec2 unit = 1.0 / vec2(textureSize(tbx_local_shadow_atlas, 0).xy);
    vec2 texel = unit * max(shadowSettings.z, 1.0);
    float sum = 0.0;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
        {
            float closest =
                texture(tbx_local_shadow_atlas, vec3(proj.xy + (vec2(x, y) * texel), float(layer))).r;
            sum += current <= closest ? 1.0 : 0.0;
        }
    return sum * (1.0 / 9.0);
}

// Local (point/spot/area) light occlusion (0 = shadowed, 1 = lit). Selects the light's atlas layer
// (a point light's six faces are dispatched by the dominant axis of the light->fragment vector),
// projects the fragment by that view's matrix, and depth-compares. A texel/depth-proportional normal
// offset fights acne the same way the directional path does; fragments outside the view's frustum are
// fully lit. Returns 1 for lights that cast no shadow (shadowData.y < 0).
float tbx_local_shadow(LightData light, vec3 world_position, vec3 n)
{
    int base = int(light.shadowData.y);
    if (base < 0)
        return 1.0;

    int layer = base;
    if (int(light.shadowData.z) == 6) // point light: pick the cube face we fall into
    {
        vec3 d = world_position - light.positionRange.xyz;
        vec3 ad = abs(d);
        if (ad.x >= ad.y && ad.x >= ad.z)
            layer = base + (d.x >= 0.0 ? 0 : 1);
        else if (ad.y >= ad.z)
            layer = base + (d.y >= 0.0 ? 2 : 3);
        else
            layer = base + (d.z >= 0.0 ? 4 : 5);
    }

    // Normal-offset shadows: push the sample point off the surface by a few shadow-map texels before
    // projecting. A perspective shadow map's texels widen with distance from the light (world texel
    // ~= 2 * tan(fov/2) * dist / resolution), so the offset MUST scale with that distance — a fixed
    // tiny offset is what speckles every lit surface with self-shadow acne (the radial streaks a lamp
    // leaves on the floor below it, or a wall shadowing itself solid black). The constant covers the
    // near field where the distance term is small.
    float dist = length(world_position - light.positionRange.xyz);
    vec3 offset_position = world_position + n * (dist * 0.008 + 0.05);

    vec4 clip = localShadowMatrices[layer] * vec4(offset_position, 1.0);
    if (clip.w <= 0.0)
        return 1.0; // behind the view
    vec3 proj = (clip.xyz / clip.w) * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0;

    // A small slope-scaled depth bias mops up the residual acne on surfaces grazing the light that the
    // normal offset alone can't fully clear. Independent of the (much tighter) directional bias.
    vec3 l = normalize(light.positionRange.xyz - world_position);
    float bias = max(0.0025 * (1.0 - max(dot(n, l), 0.0)), 0.0008);
    return tbx_local_shadow_pcf(layer, proj, bias);
}

// Cook-Torrance specular lobe for one light direction (no radiance / N·L applied — the caller scales).
vec3 tbx_specular_brdf(vec3 n, vec3 v, vec3 l, float roughness, vec3 f0)
{
    vec3 h = normalize(v + l);
    float ndotv = max(dot(n, v), 0.0);
    float ndotl = max(dot(n, l), 0.0);
    float hdotv = max(dot(h, v), 0.0);
    float ndf = tbx_distribution_ggx(n, h, roughness);
    float g = tbx_geometry_smith(n, v, l, roughness);
    vec3 fresnel = tbx_fresnel_schlick(hdotv, f0);
    return (ndf * g * fresnel) / max(4.0 * ndotv * ndotl, TBX_EPSILON);
}

// Lambertian diffuse lobe, energy-balanced against the specular Fresnel (no radiance / N·L applied).
vec3 tbx_diffuse_brdf(vec3 albedo, float metallic, vec3 n, vec3 v, vec3 l, vec3 f0)
{
    vec3 h = normalize(v + l);
    float hdotv = max(dot(h, v), 0.0);
    vec3 fresnel = tbx_fresnel_schlick(hdotv, f0);
    return (vec3(1.0) - fresnel) * (1.0 - metallic) * albedo * TBX_INV_PI;
}

// Rectangular area light via the "most representative point" approximation (Karis/UE4): the diffuse
// term is lit from the closest point on the rectangle, while the specular term is lit from where the
// view's reflection ray meets the rectangle — so highlights stretch and soften with the panel's size
// (the look a point light can't produce) instead of collapsing to a pin-prick. The emitter is
// one-sided: it only lights fragments in front of its plane. The rect's in-plane orientation is rebuilt
// from its emission normal assuming a world-up-aligned roll (the engine doesn't author softbox roll).
// Size lives in spotAnglesArea.zw (width, height). Returns the light's full diffuse+specular term.
vec3 tbx_shade_area_light(
    LightData light,
    vec3 albedo, float metallic, float roughness, vec3 f0,
    vec3 n, vec3 world_position, vec3 v)
{
    vec3 center = light.positionRange.xyz;
    vec3 forward = normalize(light.directionType.xyz);
    vec3 to_frag = world_position - center;
    float front = dot(to_frag, forward);
    if (front <= 0.0)
        return vec3(0.0); // behind the one-sided emitter

    vec3 up_ref = abs(forward.y) > 0.99 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up_ref, forward));
    vec3 up = cross(forward, right);
    float half_w = max(light.spotAnglesArea.z * 0.5, TBX_EPSILON);
    float half_h = max(light.spotAnglesArea.w * 0.5, TBX_EPSILON);

    // Diffuse representative point: clamp the fragment's projection onto the rect plane to the rect.
    vec3 in_plane = to_frag - front * forward;
    vec3 diffuse_pt = center + clamp(dot(in_plane, right), -half_w, half_w) * right
                      + clamp(dot(in_plane, up), -half_h, half_h) * up;
    vec3 ld_vec = diffuse_pt - world_position;
    float ld_dist2 = dot(ld_vec, ld_vec);
    vec3 ld = ld_vec * inversesqrt(max(ld_dist2, TBX_EPSILON));

    // Specular representative point: intersect the view reflection with the rect plane, clamp to rect.
    vec3 r = reflect(-v, n);
    float denom = dot(r, forward);
    vec3 spec_pt = diffuse_pt;
    if (abs(denom) > TBX_EPSILON)
    {
        vec3 hit = world_position + r * max(dot(center - world_position, forward) / denom, 0.0);
        vec3 dh = hit - center;
        spec_pt = center + clamp(dot(dh, right), -half_w, half_w) * right
                  + clamp(dot(dh, up), -half_h, half_h) * up;
    }
    vec3 ls_vec = spec_pt - world_position;
    float ls_dist2 = dot(ls_vec, ls_vec);
    vec3 ls = ls_vec * inversesqrt(max(ls_dist2, TBX_EPSILON));

    // Widen the specular lobe by the panel's apparent size so a big/near area light blurs its highlight.
    float light_radius = 0.5 * (half_w + half_h);
    float spec_rough = clamp(roughness + light_radius * inversesqrt(max(ls_dist2, 1.0)) * 0.5, roughness, 1.0);

    vec3 color = light.colorIntensity.rgb * light.colorIntensity.a;
    vec3 diffuse = tbx_diffuse_brdf(albedo, metallic, n, v, ld, f0)
                   * (tbx_distance_attenuation(ld_dist2, light.positionRange.w) * max(dot(n, ld), 0.0));
    vec3 specular = tbx_specular_brdf(n, v, ls, spec_rough, f0)
                    * (tbx_distance_attenuation(ls_dist2, light.positionRange.w) * max(dot(n, ls), 0.0));
    return (diffuse + specular) * color;
}

vec3 tbx_shade_pbr(
    vec3 albedo,
    float metallic,
    float roughness,
    float ao,
    vec3 normal,
    vec3 world_position,
    vec3 view_direction)
{
    roughness = clamp(roughness, 0.04, 1.0);
    vec3 n = normalize(normal);
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    float ndotv = max(dot(n, view_direction), 0.0);

    // Hemispheric ambient: scale the accumulated ambient (sky) irradiance by how much the surface
    // faces up, fading toward a dimmer "ground bounce" underneath. A flat constant fill is the main
    // reason untextured surfaces read as flat; a normal-driven gradient restores form.
    float hemi = n.y * 0.5 + 0.5;
    vec3 ambient_irradiance = ambientLight.rgb * mix(0.35, 1.0, hemi);

    // Ambient specular stands in for image-based lighting so smooth metals reflect the surrounding
    // ambient instead of reading as black; occluded (AO/cavity) areas are damped by spec occlusion.
    vec3 ambient_fresnel = tbx_fresnel_schlick_roughness(ndotv, f0, roughness);
    float spec_ao = tbx_specular_occlusion(ndotv, ao, roughness);
    vec3 diffuse_ambient = (vec3(1.0) - ambient_fresnel) * (1.0 - metallic) * albedo;
    vec3 color = (diffuse_ambient * ao + ambient_fresnel * spec_ao) * ambient_irradiance;

    for (uint i = 0u; i < lightCount; ++i)
    {
        LightData light = lights[i];
        uint type = uint(light.directionType.w);

        // Area lights drive diffuse and specular from separate representative points, so they get
        // their own evaluator instead of the single-direction path below. Their shadow map (a single
        // atlas view facing the emitter normal) gates the whole contribution so they don't bleed
        // through walls.
        if (type == TBX_SHADER_LIGHT_TYPE_AREA)
        {
            float area_shadow = tbx_local_shadow(light, world_position, n);
            if (area_shadow > 0.0)
                color += area_shadow
                         * tbx_shade_area_light(
                             light, albedo, metallic, roughness, f0, n, world_position, view_direction);
            continue;
        }

        vec3 l = tbx_light_direction(light, world_position);
        float ndotl = max(dot(n, l), 0.0);
        if (ndotl <= 0.0)
            continue;

        float attenuation =
            tbx_light_attenuation(light, world_position) * tbx_spotlight_factor(light, world_position);

        // Occlusion-test the light against its shadow map so it no longer lights surfaces it can't
        // physically reach (interiors, the far side of walls). The directional caster (shadowData.x >=
        // 0) returns a colored factor — opaque geometry darkens it, transparent casters tint it; point
        // and spot lights (shadowData.y >= 0) sample the local atlas for a monochrome occlusion factor.
        vec3 shadow = vec3(1.0);
        if (type == TBX_SHADER_LIGHT_TYPE_DIRECTIONAL && light.shadowData.x >= 0.0)
            shadow = tbx_directional_shadow(world_position, n, l);
        else if (light.shadowData.y >= 0.0)
            shadow = vec3(tbx_local_shadow(light, world_position, n));

        vec3 diffuse = tbx_diffuse_brdf(albedo, metallic, n, view_direction, l, f0);
        vec3 specular = tbx_specular_brdf(n, view_direction, l, roughness, f0);
        vec3 radiance = light.colorIntensity.rgb * light.colorIntensity.a * attenuation * shadow;
        color += (diffuse + specular) * radiance * ndotl;
    }
    return color;
}
