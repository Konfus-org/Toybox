# Material Shader Contract Design

## Summary

Toybox should treat `Material` and `MaterialInstance` data as the authored source of truth for
shader-facing inputs. The rendering pipeline should stop owning built-in material property names
such as `albedo_color` or `roughness_map`. Instead, each `ShaderProgram` should declare the
material contract it expects through authored binding metadata. The renderer validates the
effective material against that contract before upload. Missing required bindings or incompatible
parameter types cause the draw to fall back to the magenta fallback material. Extra material
bindings that the shader does not declare produce warnings and are not uploaded.

This design keeps the pipeline data-driven without adding runtime shader reflection.

## Goals

- Remove renderer-owned built-in PBR material parameter names from the generic pipeline.
- Let shaders declare the material property and texture names they consume.
- Keep `Material` and `MaterialInstance` free-form and name-based.
- Validate authored material data deterministically before rendering.
- Fall back to the magenta material when a material cannot satisfy its shader contract.
- Warn on unused material bindings without failing the draw.

## Non-Goals

- Runtime shader reflection.
- Per-shader handwritten adapter code.
- Automatic inference of shader contracts from GLSL source.
- Allowing the renderer to guess partial bindings for malformed materials.

## Current State

`Material` already stores arbitrary named parameters and textures. `MaterialInstance` already
supports override-by-name and override-by-id.

The rigid part of the current system is the rendering pipeline. It assumes a fixed PBR schema and
packs material data into `ShaderMaterialData` by looking up hard-coded names such as
`albedo_color`, `emissive_color`, `metallic`, `roughness`, and `skybox_texture`. This makes the
pipeline materially less flexible than the asset model.

## Design

### Shader-authored contract

Extend `ShaderProgram` with authored material binding metadata. Each entry declares:

- `name`: binding name without engine-specific prefixes.
- `kind`: parameter or texture.
- `type`: expected parameter type for parameter bindings.
- `required`: whether the binding must exist to render with this shader.

The binding names are the public shader contract. Materials are expected to expose properties and
textures under those same names.

### Material remains the source of truth

`Material` and `MaterialInstance` remain flexible containers of named values. The renderer does not
normalize, rename, or reinterpret material properties into a built-in schema. The effective
material view is still produced by overlaying instance overrides on top of the base material asset.

### Validation

Before a material is uploaded for a draw, the renderer validates the effective material bindings
against the shader program contract.

Validation rules:

- Missing required texture binding: fatal for the draw.
- Missing required parameter binding: fatal for the draw.
- Parameter type mismatch: fatal for the draw.
- Extra material parameter not declared by the shader: warning, ignored.
- Extra material texture not declared by the shader: warning, ignored.

Fatal validation failures log a warning that includes the material, shader, and offending binding.
The renderer then substitutes the fallback magenta material for that draw instead of trying to
render with a partially valid material.

### Upload behavior

After validation succeeds, the renderer uploads only the bindings declared by the shader contract.
Binding lookup remains name/id based through the existing material APIs, but the names now come from
shader metadata rather than renderer constants.

The renderer should not upload material bindings that the shader does not declare, even when they
exist on the material asset.

### Fallback behavior

The fallback magenta material remains the safety net for invalid authored content. It must satisfy
its own shader contract completely so fallback rendering is deterministic.

If a non-fallback material fails validation:

1. Emit a warning once for the failure reason.
2. Replace that material usage with the fallback material.
3. Validate and upload the fallback material using the same contract-driven path.

If the fallback material itself is invalid, the renderer should emit a hard warning and continue
with the existing failure presentation path rather than attempting recursive fallback substitution.

## Data Model Changes

### Shader asset types

Add material binding descriptors to `ShaderProgram`. The shader asset does not need reflection or
GPU binding location data for this design. It only needs authored material contract metadata.

Representative shape:

```cpp
enum class ShaderMaterialBindingKind : uint8_t
{
    PARAMETER,
    TEXTURE
};

enum class ShaderMaterialParameterType : uint8_t
{
    BOOL,
    INT,
    FLOAT,
    DOUBLE,
    VEC2,
    VEC3,
    VEC4,
    COLOR,
    MAT3,
    MAT4
};

struct ShaderMaterialBinding
{
    std::string name = "";
    ShaderMaterialBindingKind kind = ShaderMaterialBindingKind::PARAMETER;
    ShaderMaterialParameterType type = ShaderMaterialParameterType::FLOAT;
    bool required = true;
};
```

The exact names can change, but the contract needs equivalent information.

### Rendering pipeline helpers

Add helpers to:

- Build an effective material view from base material plus overrides.
- Validate effective bindings against a shader contract.
- Emit one-time warnings for invalid or unused bindings.
- Resolve fallback substitution without reintroducing hard-coded material names.

## Renderer Boundary

The generic renderer should continue to own:

- Frame scheduling and render pass order.
- Global scene, instance, light, and mesh GPU buffer layouts.
- Fixed engine ABI for pass resources declared in `shader_bindings.h`.
- Validation and upload mechanics for shader-authored material contracts.

The renderer should no longer own:

- Material property names such as `albedo_color` or `metallic`.
- Texture names such as `normal_map` or `skybox_texture`.
- Any built-in assumption that every scene shader is PBR-shaped.

## Testing

Add focused unit coverage around validation behavior.

Required positive and negative behavioral pairs:

- Positive: material matching the shader contract validates and uploads successfully.
- Negative: material missing a required parameter falls back to magenta.
- Positive: optional binding omitted still validates.
- Negative: parameter type mismatch falls back to magenta.
- Positive: extra undeclared binding logs warning and the declared bindings still upload.
- Negative: invalid fallback material surfaces a hard failure path instead of recursive fallback.

Tests should instantiate the real validation path and use backend/asset fakes rather than mocking
the system under test.

## Migration Plan

1. Add shader-authored material binding metadata types to the shader asset model.
2. Author contract metadata for the existing standard materials and shaders.
3. Replace the hard-coded material-name extraction path in `RenderingPipeline` with validation and
   contract-driven upload.
4. Keep the existing global pass ABI untouched so the refactor stays scoped to material/shader
   binding behavior.
5. Update shader pipeline documentation to describe the new contract-driven material path.

## Risks

- Existing materials and shaders will need matching contract metadata before the pipeline can render
  them through the new path.
- Current GPU-side material packing may still reflect a fixed buffer shape. If so, the first
  implementation may need to support authored contracts within that existing packed layout before a
  fuller generic upload path is introduced.
- Warning spam is likely if content authors rely on shared material assets with many unused
  properties. One-time warning suppression should be included from the start.

## Recommendation

Implement shader-authored material contracts now and keep the first pass scoped to validation,
warning behavior, fallback substitution, and contract-driven lookup. Do not attempt full shader
reflection or a broad redesign of the frame-level GPU ABI in the same change.
