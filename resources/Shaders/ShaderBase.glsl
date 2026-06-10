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
#define TBX_SHADER_BINDING_SHADOW_CASCADE_0 12
#define TBX_SHADER_BINDING_SHADOW_CASCADE_1 13
#define TBX_SHADER_BINDING_SHADOW_CASCADE_2 14
#define TBX_SHADER_BINDING_SHADOW_CASCADE_3 15
#define TBX_SHADER_BINDING_SHADOW_COLOR 16
#define TBX_SHADER_BINDING_SHADOW_PASS 6
#define TBX_SHADER_BINDING_FINAL_HDR 23

#define TBX_SHADER_LIGHT_TYPE_DIRECTIONAL 0u
#define TBX_SHADER_LIGHT_TYPE_POINT 1u
#define TBX_SHADER_LIGHT_TYPE_SPOT 2u

#define TBX_EPSILON 0.00001
#define TBX_PI 3.14159265358979323846
#define TBX_INV_PI 0.31830988618379067154
#define TBX_INV_TAU 0.15915494309189533577

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

// Directional shadow cascades (depth, each in its own light clip space). Cascade 0 is the nearest,
// highest-resolution slice; the furthest cascade is the lowest resolution and reaches the configured
// shadow_render_distance. The forward pass picks one per fragment by camera distance. Only sampled
// when a light's shadowData.x >= 0, which the CPU sets on the frame's directional caster.
layout(binding = TBX_SHADER_BINDING_SHADOW_CASCADE_0) uniform sampler2D tbx_shadow_cascade_0;
layout(binding = TBX_SHADER_BINDING_SHADOW_CASCADE_1) uniform sampler2D tbx_shadow_cascade_1;
layout(binding = TBX_SHADER_BINDING_SHADOW_CASCADE_2) uniform sampler2D tbx_shadow_cascade_2;
layout(binding = TBX_SHADER_BINDING_SHADOW_CASCADE_3) uniform sampler2D tbx_shadow_cascade_3;

// Directional translucent shadow map (RGB transmittance, full-range/furthest-cascade clip space).
// Transparent casters multiply their tint into it; the forward pass multiplies the directional light
// by this so colored glass casts a tinted, partial shadow. Cleared to white (1 = transmissive).
layout(binding = TBX_SHADER_BINDING_SHADOW_COLOR) uniform sampler2D tbx_shadow_color;

// Mirrors GpuUniforms (std140) exactly — keep every field for ABI parity even if a given shader
// only reads a subset.
layout(std140, binding = TBX_SHADER_BINDING_SCENE_UNIFORMS) uniform TbxSceneUniforms
{
    mat4 viewProjection;
    mat4 inverseViewProjection;
    mat4 cascadeViewProjection[TBX_SHADER_CASCADE_COUNT];
    mat4 colorViewProjection;
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

float tbx_light_attenuation(LightData light, vec3 world_position)
{
    if (uint(light.directionType.w) == TBX_SHADER_LIGHT_TYPE_DIRECTIONAL)
        return 1.0;
    float range = max(light.positionRange.w, TBX_EPSILON);
    float d = length(light.positionRange.xyz - world_position);
    float falloff = tbx_saturate(1.0 - (d / range));
    return falloff * falloff;
}

float tbx_spotlight_factor(LightData light, vec3 world_position)
{
    if (uint(light.directionType.w) != TBX_SHADER_LIGHT_TYPE_SPOT)
        return 1.0;
    vec3 to_fragment = normalize(world_position - light.positionRange.xyz);
    float cos_theta = dot(normalize(light.directionType.xyz), to_fragment);
    float inner_cos = cos(radians(light.spotAnglesArea.x));
    float outer_cos = cos(radians(light.spotAnglesArea.y));
    float t = tbx_saturate((cos_theta - outer_cos) / max(inner_cos - outer_cos, TBX_EPSILON));
    return t * t; // square for a soft, non-linear cone edge instead of a hard ring
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
    vec2 texel = (1.0 / vec2(textureSize(shadow_map, 0))) * max(shadowSettings.z, 1.0);
    float sum = 0.0;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
        {
            float closest = texture(shadow_map, proj.xy + (vec2(x, y) * texel)).r;
            sum += current <= closest ? 1.0 : 0.0;
        }
    return sum * (1.0 / 9.0);
}

// Directional shadow as a colored visibility factor (vec3, 1 = fully lit). Picks the cascade whose
// split radius still contains the fragment (nearest = sharpest), projects into that cascade's light
// clip space, PCF-compares against its depth map (slope-scaled bias widened for coarse far cascades),
// then multiplies by the transmittance sampled from the full-range translucent map so light through
// tinted glass is colored. Fragments outside all cascades are fully lit (white).
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

    vec4 light_clip = cascadeViewProjection[cascade] * vec4(world_position, 1.0);
    vec3 proj = (light_clip.xyz / light_clip.w) * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return vec3(1.0);

    // Coarser far cascades cover more world per texel, so widen their bias to avoid acne.
    float bias = max(shadowSettings.x * (1.0 - dot(n, l)), shadowSettings.y) * (1.0 + float(cascade));

    float opaque_visibility;
    if (cascade == 0)
        opaque_visibility = tbx_shadow_pcf(tbx_shadow_cascade_0, proj, bias);
    else if (cascade == 1)
        opaque_visibility = tbx_shadow_pcf(tbx_shadow_cascade_1, proj, bias);
    else if (cascade == 2)
        opaque_visibility = tbx_shadow_pcf(tbx_shadow_cascade_2, proj, bias);
    else
        opaque_visibility = tbx_shadow_pcf(tbx_shadow_cascade_3, proj, bias);

    // Transmittance of any transparent casters, sampled in the full-range clip space the color map
    // was rendered with (the furthest cascade) so it covers the whole shadowed range.
    vec3 transmittance = vec3(1.0);
    vec4 color_clip = colorViewProjection * vec4(world_position, 1.0);
    vec3 color_proj = (color_clip.xyz / color_clip.w) * 0.5 + 0.5;
    if (color_proj.x >= 0.0 && color_proj.x <= 1.0 && color_proj.y >= 0.0 && color_proj.y <= 1.0)
        transmittance = texture(tbx_shadow_color, color_proj.xy).rgb;
    return opaque_visibility * transmittance;
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
        vec3 l = tbx_light_direction(light, world_position);
        float ndotl = max(dot(n, l), 0.0);
        if (ndotl <= 0.0)
            continue;

        float attenuation =
            tbx_light_attenuation(light, world_position) * tbx_spotlight_factor(light, world_position);

        // The directional caster (shadowData.x >= 0) is occlusion-tested against the shadow map so
        // it no longer lights surfaces it can't physically reach (interiors, the far side of walls).
        // The test returns a colored factor: opaque geometry darkens it, transparent casters tint it.
        vec3 shadow = vec3(1.0);
        if (uint(light.directionType.w) == TBX_SHADER_LIGHT_TYPE_DIRECTIONAL
            && light.shadowData.x >= 0.0)
            shadow = tbx_directional_shadow(world_position, n, l);

        vec3 h = normalize(view_direction + l);
        float hdotv = max(dot(h, view_direction), 0.0);
        float ndf = tbx_distribution_ggx(n, h, roughness);
        float g = tbx_geometry_smith(n, view_direction, l, roughness);
        vec3 fresnel = tbx_fresnel_schlick(hdotv, f0);
        vec3 specular = (ndf * g * fresnel) / max(4.0 * ndotv * ndotl, TBX_EPSILON);
        vec3 diffuse = (vec3(1.0) - fresnel) * (1.0 - metallic) * albedo * TBX_INV_PI;
        vec3 radiance = light.colorIntensity.rgb * light.colorIntensity.a * attenuation * shadow;
        color += (diffuse + specular) * radiance * ndotl;
    }
    return color;
}
