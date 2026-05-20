# Shader Pipeline

This document summarizes a recommended shader structure for Toybox based on the current C++ material setup:

- `Material` owns a `ShaderProgram`, default named parameters, default named textures, and render config.
- `MaterialInstance` owns runtime parameter overrides, texture overrides, optional config override, and dirty state.
- The C++ side is responsible for resolving defaults plus overrides, packing material data into the shader ABI, and binding textures/buffers.
- The shader side should focus on stable layout contracts, small reusable includes, and user-owned `main()` functions.

The core rule:

```txt
Base files provide assumptions.
Material files provide ABI.
Lighting files provide algorithms.
User shaders provide main().
```

---

## Core Idea

Every shader should generally look like this:

```glsl
#include "ShaderBase.glsl"

// Shader uniforms here

void main()
{
    // User/domain-specific logic here.
}
```

Toybox provides:

```txt
Frame data
Camera data
Object data
Material data
Texture bindings
Lighting buffers
Shadow maps
Utility functions
Common structs
```

The shader author provides:

```txt
main()
Surface construction
Final output behavior
Any custom effects
```

---

## Naming Conventions

Use common GLSL/shader naming conventions.

### Files

Use PascalCase for reusable include files and shader asset files:

```txt
ShaderCommon.glsl
ShaderVertBase.glsl
ShaderFragBase.glsl
SceneBindings.glsl
PbrMaterial.glsl
UnlitMaterial.glsl
LitMaterial.glsl
PbrLighting.glsl
ShadowSampling.glsl
PostProcessBase.glsl
DefaultPbr.frag
DefaultUnlit.frag
```

### Macros and Constants

Use uppercase snake case:

```glsl
#define TBX_PI 3.14159265359
#define TBX_EPSILON 0.00001

#define TBX_BINDING_FRAME_DATA 0
#define TBX_BINDING_CAMERA_DATA 1
#define TBX_BINDING_OBJECT_DATA 2
#define TBX_BINDING_MATERIAL_DATA 3
```

### Uniform Variables

Use `u_` prefix with lower snake case:

```glsl
uniform mat4 u_view;
uniform mat4 u_projection;
uniform mat4 u_view_projection;
uniform vec4 u_camera_world_position;

uniform vec4 u_albedo_color;
uniform vec4 u_params0;
uniform sampler2D u_albedo_map;
```

### Vertex Attributes

Use `a_` prefix with lower snake case:

```glsl
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_tangent;
layout(location = 3) in vec2 a_tex_coord;
layout(location = 4) in vec4 a_color;
```

### Varyings

Use `v_` prefix with lower snake case:

```glsl
layout(location = 0) out vec3 v_world_position;
layout(location = 1) out vec3 v_world_normal;
layout(location = 2) out vec4 v_world_tangent;
layout(location = 3) out vec2 v_tex_coord;
layout(location = 4) out vec4 v_color;
```

### Fragment Outputs

Use `o_` prefix with lower snake case:

```glsl
layout(location = 0) out vec4 o_color;
```

### Functions

Use lower snake case:

```glsl
float saturate(float value);
vec3 tbx_sample_normal(vec2 uv, vec3 normal, vec4 tangent);
PbrSurface tbx_build_pbr_surface(vec3 world_position, vec3 world_normal, vec4 world_tangent, vec2 uv);
vec3 tbx_shade_pbr(PbrSurface surface);
```

### Structs

Use PascalCase for type names and lower snake case for members:

```glsl
struct PbrSurface
{
    vec3 world_position;
    vec3 normal;
    vec3 albedo;
    vec3 emissive;

    float alpha;
    float metallic;
    float roughness;
    float ao;
};
```

### Uniform Blocks

Use PascalCase for block names and lower snake case for members:

```glsl
layout(std140, binding = TBX_BINDING_CAMERA_DATA) uniform TbxCameraData
{
    mat4 u_view;
    mat4 u_projection;
    mat4 u_view_projection;
    mat4 u_inverse_view;
    mat4 u_inverse_projection;
    vec4 u_camera_world_position;
};
```

### Packed Parameter Vectors

Use generic packed names like `u_params0`, `u_params1`, etc., but expose readable aliases when useful:

```glsl
layout(std140, binding = TBX_BINDING_MATERIAL_DATA) uniform TbxPbrMaterialData
{
    vec4 u_albedo_color;

    // x = metallic
    // y = roughness
    // z = normal_strength
    // w = ao
    vec4 u_params0;

    vec4 u_emissive_color;
};

#define u_metallic        u_params0.x
#define u_roughness       u_params0.y
#define u_normal_strength u_params0.z
#define u_ao              u_params0.w
```

This keeps the GPU layout compact while still making shader code readable.

---

## Base Shader Files

### `ShaderCommon.glsl`

Contains constants, binding slots, and general utility functions valid everywhere.

```glsl
#define TBX_PI 3.14159265359
#define TBX_EPSILON 0.00001

#define TBX_BINDING_FRAME_DATA      0
#define TBX_BINDING_CAMERA_DATA     1
#define TBX_BINDING_OBJECT_DATA     2
#define TBX_BINDING_MATERIAL_DATA   3

#define TBX_BINDING_ALBEDO_MAP              10
#define TBX_BINDING_NORMAL_MAP              11
#define TBX_BINDING_METALLIC_ROUGHNESS_MAP  12
#define TBX_BINDING_EMISSIVE_MAP            13

#define TBX_BINDING_LIGHT_DATA      20
#define TBX_BINDING_SHADOW_MAP      30

float saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

vec3 saturate(vec3 value)
{
    return clamp(value, vec3(0.0), vec3(1.0));
}
```

---

### `SceneBindings.glsl`

This is the global Toybox scene contract.

```glsl
#include "ShaderCommon.glsl"

layout(std140, binding = TBX_BINDING_FRAME_DATA) uniform TbxFrameData
{
    float u_time;
    float u_delta_time;
    vec2  u_viewport_size;
};

layout(std140, binding = TBX_BINDING_CAMERA_DATA) uniform TbxCameraData
{
    mat4 u_view;
    mat4 u_projection;
    mat4 u_view_projection;
    mat4 u_inverse_view;
    mat4 u_inverse_projection;
    vec4 u_camera_world_position;
};

layout(std140, binding = TBX_BINDING_OBJECT_DATA) uniform TbxObjectData
{
    mat4 u_model;
    mat4 u_normal_matrix;
};
```

---

### `ShaderVertBase.glsl`

For regular mesh vertex shaders.

```glsl
#include "ShaderCommon.glsl"
#include "SceneBindings.glsl"

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_tangent;
layout(location = 3) in vec2 a_tex_coord;
layout(location = 4) in vec4 a_color;

layout(location = 0) out vec3 v_world_position;
layout(location = 1) out vec3 v_world_normal;
layout(location = 2) out vec4 v_world_tangent;
layout(location = 3) out vec2 v_tex_coord;
layout(location = 4) out vec4 v_color;

void tbx_default_vertex()
{
    vec4 world_position = u_model * vec4(a_position, 1.0);

    v_world_position = world_position.xyz;
    v_world_normal = normalize((u_normal_matrix * vec4(a_normal, 0.0)).xyz);
    v_world_tangent = vec4(normalize((u_model * vec4(a_tangent.xyz, 0.0)).xyz), a_tangent.w);
    v_tex_coord = a_tex_coord;
    v_color = a_color;

    gl_Position = u_view_projection * world_position;
}
```

A simple default vertex shader can be:

```glsl
#version 450

#include "Toybox/Base/ShaderVertBase.glsl"

void main()
{
    tbx_default_vertex();
}
```

---

### `ShaderFragBase.glsl`

For regular mesh fragment shaders.

```glsl
#include "ShaderCommon.glsl"
#include "SceneBindings.glsl"

layout(location = 0) in vec3 v_world_position;
layout(location = 1) in vec3 v_world_normal;
layout(location = 2) in vec4 v_world_tangent;
layout(location = 3) in vec2 v_tex_coord;
layout(location = 4) in vec4 v_color;

layout(location = 0) out vec4 o_color;
```

---

## PBR Shader Shape

A PBR shader should build a `PbrSurface`, then pass it to lighting.

Use PBR for standard physically based 3D surfaces.

```txt
Includes:
    ShaderFragBase
    PbrMaterial
    PbrLighting

Owns:
    Surface modifications
    Final output

Requires:
    Albedo
    Normal
    Metallic/Roughness
    Emissive optional
```

---

### `PbrMaterial.glsl`

```glsl
#include "../Base/ShaderCommon.glsl"

struct PbrSurface
{
    vec3 world_position;
    vec3 normal;
    vec3 albedo;
    vec3 emissive;

    float alpha;
    float metallic;
    float roughness;
    float ao;
};

layout(std140, binding = TBX_BINDING_MATERIAL_DATA) uniform TbxPbrMaterialData
{
    vec4 u_albedo_color;

    // x = metallic
    // y = roughness
    // z = normal_strength
    // w = ao
    vec4 u_params0;

    vec4 u_emissive_color;
};

layout(binding = TBX_BINDING_ALBEDO_MAP)
uniform sampler2D u_albedo_map;

layout(binding = TBX_BINDING_NORMAL_MAP)
uniform sampler2D u_normal_map;

layout(binding = TBX_BINDING_METALLIC_ROUGHNESS_MAP)
uniform sampler2D u_metallic_roughness_map;

layout(binding = TBX_BINDING_EMISSIVE_MAP)
uniform sampler2D u_emissive_map;

#define u_metallic        u_params0.x
#define u_roughness       u_params0.y
#define u_normal_strength u_params0.z
#define u_ao              u_params0.w

vec3 tbx_sample_normal(vec2 uv, vec3 normal, vec4 tangent)
{
    vec3 n = normalize(normal);
    vec3 t = normalize(tangent.xyz);
    vec3 b = normalize(cross(n, t) * tangent.w);

    mat3 tbn = mat3(t, b, n);

    vec3 sampled = texture(u_normal_map, uv).xyz;
    sampled = sampled * 2.0 - 1.0;
    sampled.xy *= u_normal_strength;

    return normalize(tbn * sampled);
}

PbrSurface tbx_build_pbr_surface(
    vec3 world_position,
    vec3 world_normal,
    vec4 world_tangent,
    vec2 uv)
{
    vec4 albedo_sample = texture(u_albedo_map, uv);
    vec4 mr_sample = texture(u_metallic_roughness_map, uv);
    vec4 emissive_sample = texture(u_emissive_map, uv);

    PbrSurface surface;

    surface.world_position = world_position;
    surface.normal = tbx_sample_normal(uv, world_normal, world_tangent);
    surface.albedo = albedo_sample.rgb * u_albedo_color.rgb;
    surface.alpha = albedo_sample.a * u_albedo_color.a;

    surface.metallic = clamp(u_metallic * mr_sample.b, 0.0, 1.0);
    surface.roughness = clamp(u_roughness * mr_sample.g, 0.04, 1.0);
    surface.ao = clamp(u_ao, 0.0, 1.0);

    surface.emissive = emissive_sample.rgb * u_emissive_color.rgb;

    return surface;
}
```

---

### `DefaultPbr.frag`

```glsl
#version 450

#include "Toybox/Base/ShaderFragBase.glsl"
#include "Toybox/Materials/PbrMaterial.glsl"
#include "Toybox/Lighting/PbrLighting.glsl"

void main()
{
    PbrSurface surface = tbx_build_pbr_surface(
        v_world_position,
        normalize(v_world_normal),
        v_world_tangent,
        v_tex_coord);

    vec3 color = tbx_shade_pbr(surface);
    color += surface.emissive;

    o_color = vec4(color, surface.alpha);
}
```

---

### Custom PBR Shader

A custom PBR shader should use the same material and lighting contract, but customize how the surface is modified or output.

```glsl
#version 450

#include "Toybox/Base/ShaderFragBase.glsl"
#include "Toybox/Materials/PbrMaterial.glsl"
#include "Toybox/Lighting/PbrLighting.glsl"

void main()
{
    PbrSurface surface = tbx_build_pbr_surface(
        v_world_position,
        normalize(v_world_normal),
        v_world_tangent,
        v_tex_coord);

    float pulse = sin(u_time * 4.0) * 0.5 + 0.5;
    surface.emissive += surface.albedo * pulse * 0.25;

    vec3 color = tbx_shade_pbr(surface);
    color += surface.emissive;

    o_color = vec4(color, surface.alpha);
}
```

---

## Lit Shader Shape

Use Lit for simpler non-PBR lighting.

```txt
PBR:
    Physically based material.
    Metallic/roughness workflow.
    Used for most 3D world geometry.

Lit:
    Simpler lighting model.
    Diffuse/specular or stylized lighting.
    Good for toon, simple props, low-end/mobile, debug, stylized games.
```

```txt
Includes:
    ShaderFragBase
    LitMaterial
    SimpleLighting

Requires:
    Main texture/color
    Normal
    Simple specular/smoothness optional
```

---

### `LitMaterial.glsl`

```glsl
#include "../Base/ShaderCommon.glsl"

struct LitSurface
{
    vec3 world_position;
    vec3 normal;
    vec3 color;
    vec3 emissive;

    float alpha;
    float specular_strength;
    float smoothness;
};

layout(std140, binding = TBX_BINDING_MATERIAL_DATA) uniform TbxLitMaterialData
{
    vec4 u_color;

    // x = specular_strength
    // y = smoothness
    // z = normal_strength
    // w = unused
    vec4 u_params0;

    vec4 u_emissive_color;
};

layout(binding = TBX_BINDING_ALBEDO_MAP)
uniform sampler2D u_main_tex;

layout(binding = TBX_BINDING_NORMAL_MAP)
uniform sampler2D u_normal_map;

#define u_specular_strength u_params0.x
#define u_smoothness        u_params0.y
#define u_normal_strength   u_params0.z

LitSurface tbx_build_lit_surface(
    vec3 world_position,
    vec3 world_normal,
    vec2 uv)
{
    vec4 sample_color = texture(u_main_tex, uv) * u_color;

    LitSurface surface;
    surface.world_position = world_position;
    surface.normal = normalize(world_normal);
    surface.color = sample_color.rgb;
    surface.alpha = sample_color.a;
    surface.specular_strength = u_specular_strength;
    surface.smoothness = u_smoothness;
    surface.emissive = u_emissive_color.rgb;

    return surface;
}
```

---

### `DefaultLit.frag`

```glsl
#version 450

#include "Toybox/Base/ShaderFragBase.glsl"
#include "Toybox/Materials/LitMaterial.glsl"
#include "Toybox/Lighting/SimpleLighting.glsl"

void main()
{
    LitSurface surface = tbx_build_lit_surface(
        v_world_position,
        normalize(v_world_normal),
        v_tex_coord);

    vec3 color = tbx_shade_lit(surface);
    color += surface.emissive;

    o_color = vec4(color, surface.alpha);
}
```

---

## Unlit Shader Shape

Use Unlit for sprites, UI, debug, gizmos, simple emissive objects, and flat-color objects.

```txt
Includes:
    ShaderFragBase
    UnlitMaterial

Does not require:
    Lights
    Shadows
    PBR data
```

---

### `UnlitMaterial.glsl`

```glsl
#include "../Base/ShaderCommon.glsl"

struct UnlitSurface
{
    vec3 color;
    float alpha;
};

layout(std140, binding = TBX_BINDING_MATERIAL_DATA) uniform TbxUnlitMaterialData
{
    vec4 u_color;

    // x = alpha_cutoff
    // yzw = unused
    vec4 u_params0;
};

layout(binding = TBX_BINDING_ALBEDO_MAP)
uniform sampler2D u_main_tex;

#define u_alpha_cutoff u_params0.x

UnlitSurface tbx_build_unlit_surface(vec2 uv, vec4 vertex_color)
{
    vec4 sample_color = texture(u_main_tex, uv) * u_color * vertex_color;

    UnlitSurface surface;
    surface.color = sample_color.rgb;
    surface.alpha = sample_color.a;

    return surface;
}
```

---

### `DefaultUnlit.frag`

```glsl
#version 450

#include "Toybox/Base/ShaderFragBase.glsl"
#include "Toybox/Materials/UnlitMaterial.glsl"

void main()
{
    UnlitSurface surface = tbx_build_unlit_surface(v_tex_coord, v_color);

    if (surface.alpha < u_alpha_cutoff)
    {
        discard;
    }

    o_color = vec4(surface.color, surface.alpha);
}
```

---

## Custom Shader Shape

Use a custom shader when the user wants full control.

```txt
Includes:
    ShaderVertBase or ShaderFragBase as needed

Owns:
    Material UBO meaning
    Texture usage
    main()

Does not require:
    PBR structs
    Lighting helpers
```

A custom mesh fragment shader can look like this:

```glsl
#version 450

#include "Toybox/Base/ShaderFragBase.glsl"

layout(std140, binding = TBX_BINDING_MATERIAL_DATA) uniform CustomMaterialData
{
    vec4 u_color;
    vec4 u_params0;
};

layout(binding = TBX_BINDING_ALBEDO_MAP)
uniform sampler2D u_main_tex;

void main()
{
    vec4 tex = texture(u_main_tex, v_tex_coord);

    float stripe = step(0.5, fract(v_tex_coord.y * u_params0.x));

    vec3 color = mix(u_color.rgb, tex.rgb, stripe);

    o_color = vec4(color, tex.a * u_color.a);
}
```

The minimum promise is:

```txt
Toybox gives you common scene/object/material bindings.
You decide what the material data means.
```

For fully custom shaders, allow users to define their own material UBO, but strongly recommend using `TBX_BINDING_MATERIAL_DATA`.

---

## Post-Processing Shader Shape

Post shaders are different. They usually do not need object data, vertex attributes, material textures, or lighting.

They need:

```txt
Fullscreen triangle
Source color texture
Depth texture maybe
Camera inverse matrices maybe
Post material settings
```

Post shaders should generally be:

```txt
input texture(s)
post material data
fullscreen output
```

No mesh surface, no object transform.

---

### `PostProcessBase.glsl`

```glsl
#include "../Base/ShaderCommon.glsl"
#include "../Base/SceneBindings.glsl"

#define TBX_BINDING_POST_SOURCE_COLOR 40
#define TBX_BINDING_POST_SOURCE_DEPTH 41

layout(binding = TBX_BINDING_POST_SOURCE_COLOR)
uniform sampler2D u_source_color;

layout(binding = TBX_BINDING_POST_SOURCE_DEPTH)
uniform sampler2D u_source_depth;

layout(location = 0) in vec2 v_tex_coord;
layout(location = 0) out vec4 o_color;
```

---

### Fullscreen Vertex Shader

```glsl
#version 450

layout(location = 0) out vec2 v_tex_coord;

void main()
{
    vec2 position = vec2(
        float((gl_VertexID << 1) & 2),
        float(gl_VertexID & 2));

    v_tex_coord = position;
    gl_Position = vec4(position * 2.0 - 1.0, 0.0, 1.0);
}
```

---

### Tonemap Post Shader

```glsl
#version 450

#include "Toybox/Post/PostProcessBase.glsl"

layout(std140, binding = TBX_BINDING_MATERIAL_DATA) uniform TonemapData
{
    // x = exposure
    // y = gamma
    // zw = unused
    vec4 u_params0;
};

#define u_exposure u_params0.x
#define u_gamma    u_params0.y

vec3 tonemap_reinhard(vec3 color)
{
    return color / (color + vec3(1.0));
}

void main()
{
    vec3 hdr = texture(u_source_color, v_tex_coord).rgb;

    vec3 color = hdr * u_exposure;
    color = tonemap_reinhard(color);
    color = pow(color, vec3(1.0 / u_gamma));

    o_color = vec4(color, 1.0);
}
```

---

## Lighting Shader Shape

Lighting code should mostly be include files, not standalone shaders, unless using deferred rendering.

For forward rendering:

```txt
Surface shader builds a surface.
Lighting include shades the surface.
Fragment shader outputs the final color.
```

For deferred rendering:

```txt
Geometry pass writes GBuffer.
Lighting pass reads GBuffer.
Fullscreen lighting shader outputs final color.
```

For now, Toybox should probably start with forward PBR.

---

### `LightTypes.glsl`

```glsl
#define TBX_MAX_LIGHTS 128

struct TbxLight
{
    vec4 position_type;
    vec4 direction_range;
    vec4 color_intensity;
    vec4 params;
};

layout(std140, binding = TBX_BINDING_LIGHT_DATA) uniform TbxLightData
{
    vec4 u_ambient_color;
    int u_light_count;
    vec3 u_light_padding;

    TbxLight u_lights[TBX_MAX_LIGHTS];
};
```

---

### `PbrLighting.glsl`

```glsl
#include "../Base/ShaderCommon.glsl"
#include "../Materials/PbrMaterial.glsl"
#include "LightTypes.glsl"
#include "ShadowSampling.glsl"

vec3 tbx_shade_pbr(PbrSurface surface)
{
    vec3 view_dir = normalize(u_camera_world_position.xyz - surface.world_position);

    vec3 result = vec3(0.0);

    // Placeholder shape:
    // for each light:
    //     evaluate BRDF
    //     apply shadow
    //     accumulate

    result += surface.albedo * 0.03 * surface.ao;

    return result;
}
```

---

### Deferred Lighting Shader Shape

If Toybox eventually supports deferred rendering:

```glsl
#version 450

#include "Toybox/Post/PostProcessBase.glsl"
#include "Toybox/Lighting/PbrLighting.glsl"
#include "Toybox/Lighting/LightTypes.glsl"

layout(binding = 50) uniform sampler2D u_gbuffer_albedo;
layout(binding = 51) uniform sampler2D u_gbuffer_normal;
layout(binding = 52) uniform sampler2D u_gbuffer_material;
layout(binding = 53) uniform sampler2D u_gbuffer_depth;

void main()
{
    // Reconstruct surface from GBuffer.
    // Shade using lights.
    // Output final lighting.
}
```

---

## Shadow Shader Shape

Shadow shaders should be extremely simple. They should not include PBR lighting.

They usually only need:

```txt
Position
Object matrix
Light matrix
Optional alpha cutoff
```

They should not care about:

```txt
PBR
Lights
Camera color
Emissive
Roughness
Metallic
```

---

### `ShadowCasterVertBase.glsl`

```glsl
#include "../Base/ShaderCommon.glsl"
#include "../Base/SceneBindings.glsl"

#define TBX_BINDING_SHADOW_PASS_DATA 31

layout(std140, binding = TBX_BINDING_SHADOW_PASS_DATA) uniform TbxShadowPassData
{
    mat4 u_light_view_projection;
};

layout(location = 0) in vec3 a_position;
layout(location = 3) in vec2 a_tex_coord;

layout(location = 0) out vec2 v_tex_coord;

void tbx_default_shadow_vertex()
{
    v_tex_coord = a_tex_coord;

    vec4 world_position = u_model * vec4(a_position, 1.0);
    gl_Position = u_light_view_projection * world_position;
}
```

---

### Opaque Shadow Caster Vertex Shader

```glsl
#version 450

#include "Toybox/Shadows/ShadowCasterVertBase.glsl"

void main()
{
    tbx_default_shadow_vertex();
}
```

---

### Opaque Shadow Caster Fragment Shader

```glsl
#version 450

void main()
{
    // Depth-only.
}
```

---

### Alpha-Tested Shadow Caster Fragment Shader

For foliage, fences, sprites, etc.

```glsl
#version 450

#include "Toybox/Base/ShaderCommon.glsl"

layout(location = 0) in vec2 v_tex_coord;

layout(std140, binding = TBX_BINDING_MATERIAL_DATA) uniform ShadowMaterialData
{
    vec4 u_color;

    // x = alpha_cutoff
    // yzw = unused
    vec4 u_params0;
};

layout(binding = TBX_BINDING_ALBEDO_MAP)
uniform sampler2D u_main_tex;

#define u_alpha_cutoff u_params0.x

void main()
{
    float alpha = texture(u_main_tex, v_tex_coord).a * u_color.a;

    if (alpha < u_alpha_cutoff)
    {
        discard;
    }

    // Depth written automatically.
}
```

---

## Summary By Shader Category

### 1. Custom Shader

Use when the user wants full control.

```txt
Includes:
    ShaderVertBase or ShaderFragBase as needed

Owns:
    Material UBO meaning
    Texture usage
    main()

Does not require:
    PBR structs
    Lighting helpers
```

Shape:

```glsl
#include "Toybox/Base/ShaderFragBase.glsl"

layout(std140, binding = TBX_BINDING_MATERIAL_DATA) uniform CustomData
{
    vec4 u_color;
    vec4 u_params0;
};

void main()
{
    o_color = u_color;
}
```

---

### 2. PBR Shader

Use for standard physically based 3D surfaces.

```txt
Includes:
    ShaderFragBase
    PbrMaterial
    PbrLighting

Owns:
    Surface modifications
    Final output

Requires:
    Albedo
    Normal
    Metallic/Roughness
    Emissive optional
```

Shape:

```glsl
PbrSurface surface = tbx_build_pbr_surface(...);
vec3 color = tbx_shade_pbr(surface);
o_color = vec4(color + surface.emissive, surface.alpha);
```

---

### 3. Lit Shader

Use for simpler non-PBR lighting.

```txt
Includes:
    ShaderFragBase
    LitMaterial
    SimpleLighting

Requires:
    Main texture/color
    Normal
    Simple specular/smoothness optional
```

Shape:

```glsl
LitSurface surface = tbx_build_lit_surface(...);
vec3 color = tbx_shade_lit(surface);
o_color = vec4(color, surface.alpha);
```

---

### 4. Unlit Shader

Use for sprites, UI, debug, emissive, and simple flat objects.

```txt
Includes:
    ShaderFragBase
    UnlitMaterial

Does not require:
    Lights
    Shadows
    PBR data
```

Shape:

```glsl
UnlitSurface surface = tbx_build_unlit_surface(v_tex_coord, v_color);
o_color = vec4(surface.color, surface.alpha);
```

---

### 5. Post-Processing Shader

Use for fullscreen effects.

```txt
Includes:
    PostProcessBase

Requires:
    Source color texture
    Optional depth texture
    Optional material params

Does not require:
    Mesh vertex data
    Object transform
    Lighting surface
```

Shape:

```glsl
vec3 color = texture(u_source_color, v_tex_coord).rgb;
o_color = vec4(process(color), 1.0);
```

---

### 6. Lighting Shader / Include

Use as shared lighting code for surface shaders.

```txt
Usually include-only for forward rendering.
Standalone fullscreen pass for deferred rendering.
```

Forward shape:

```glsl
vec3 color = tbx_shade_pbr(surface);
```

Deferred shape:

```glsl
Surface surface = reconstruct_surface_from_gbuffer();
vec3 color = tbx_shade_pbr(surface);
o_color = vec4(color, 1.0);
```

---

### 7. Shadow Shader

Use for depth rendering.

```txt
Includes:
    ShadowCasterVertBase
    Optional material alpha cutoff

Does not require:
    PBR lighting
    Post processing
    Full material surface
```

Shape:

```glsl
gl_Position = u_light_view_projection * u_model * vec4(a_position, 1.0);
```

Fragment:

```glsl
// Empty for opaque.
// Alpha discard for cutout.
```

---

## Final Pattern To Standardize

Every Toybox shader should fit one of these forms:

```txt
Mesh Vertex Shader
    Base vertex input
    Object/camera data
    Writes shared varyings

Surface Fragment Shader
    Base fragment inputs
    Material include
    Optional lighting include
    User-owned main

Post Fragment Shader
    Fullscreen input
    Source textures
    Post material data
    User-owned main

Shadow Shader
    Position-only transform
    Optional alpha test
```

The practical rule:

```txt
Keep ShaderBase small.
Avoid giant conditional compilation.
Split by stage and domain.
Let Toybox provide the stable environment.
Let users own main().
```

---

## CPU Render Pipeline Shape

The renderer builds each frame in explicit stages:

```txt
Frame data
    Update frame, camera, light, and shadow shader data.

Extraction
    Read ECS render components and convert them into render items.

Batching
    Group compatible items by mesh source, material state, and pass role.
    Static and dynamic geometry both use instance data when compatible.

Upload
    Reuse stable mesh, texture, pipeline, render-target, material, uniform, and instance buffers.

Pass assembly
    Create shadow, skybox, opaque, transparent, and post passes only when they have work.

Execution
    Submit backend-neutral commands; backend implementations skip redundant state binds.
```

Shadow rendering currently uses one shadow owner per frame. Eligible local shadowed lights take
priority; directional shadows are the fallback when no local shadowed light is in range.
