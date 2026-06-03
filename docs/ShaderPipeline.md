# Shader Pipeline

Toybox now uses a GPU/data-driven render path. `RenderingPipeline` owns the frame extraction,
transform, upload, cache, dispatch, draw, and cache cleanup sequence.

`Material` remains the shader asset type. Raster materials use vertex/fragment shader stages.
The special Pipeline material is compute-only and declares its ordered stack in
`ShaderProgram.computes`.

## Fixed ABI

The fixed shader bindings are declared in `tbx/systems/graphics/shader_bindings.h`:

```txt
SHADER_BINDING_GLOBAL_ENTITIES       = 0
SHADER_BINDING_GLOBAL_MATERIALS      = 1
SHADER_BINDING_DRAW_COMMAND_LOOKUP   = 2
SHADER_BINDING_INDIRECT_COMMANDS     = 3
SHADER_BINDING_VISIBLE_ENTITY_IDS    = 4
SHADER_BINDING_SCENE_UNIFORMS        = 5
SHADER_BINDING_GLOBAL_TEXTURES       = 6
SHADER_MATERIAL_LOOKUP_STRIDE        = 1024
```

The ABI structs are `ShaderEntityData`, `ShaderMaterialData`,
`ShaderDrawIndexedIndirectCommand`, and `ShaderSceneUniforms`. Keep C++ layout explicit and
std140/std430-compatible with the GLSL declarations.

OpenGL vertex shaders must use:

```glsl
uint visible_index = gl_BaseInstance + gl_InstanceID;
uint entity_id = visibleEntityIds[visible_index];
```

Do not use Vulkan-only `gl_InstanceIndex` in OpenGL shaders.

## Frame Shape

Each frame runs this sequence:

```txt
Snapshot
    Read active world, camera, renderable entities, lights, sky, and post components.

Extract
    Read Transform + StaticMesh/DynamicMesh + MaterialInstance.

Transform
    Convert asset and runtime meshes/materials/textures into backend-ready shader data.

Upload And Cache
    Upload frame geometry, shader buffers, textures, bind groups, and pipelines.
    Reuse cached resources by stable keys.

Compute
    Run the Pipeline material compute stack in declared order.
    Reset indirect counts per pass, then cull/group visible entities.

Raster
    Submit indirect draws for shadow, opaque, and transparent passes.

Cleanup
    Destroy cached backend resources that have not been used for the configured frame window.
```

Transparent materials use the same grouped indirect path as opaque materials. V1 does not sort
transparent draws.

## Shader Authoring

Every authored shader should include a Toybox base as the first line. `Base/UniversalShaderBase.glsl`
owns the GLSL version directive plus Toybox shader binding defines, pass flags, post-process slots,
and small utility functions. Do not add a separate `#version` line to individual shader assets.

Use the narrowest base that matches the shader:

```txt
Base/UniversalShaderBase.glsl  universal version/defines/utilities
Base/SceneShaderBase.glsl      entity, visibility, and scene uniform ABI
Base/MaterialShaderBase.glsl   scene ABI plus material data and global textures
Base/PostShaderBase.glsl       fullscreen post shaders
Base/PipelineShaderBase.glsl   Pipeline material compute stack shaders
```

Mesh shaders read global entity/material buffers and the visible entity list. Fragment shaders bind
the global texture array starting at `TBX_SHADER_BINDING_GLOBAL_TEXTURES`. The v1 texture array size
is backend-limited and should not be confused with the `1024` mesh/material lookup stride.

Post-processing remains fullscreen. Sky and post materials should use the same asset, transform,
upload, and cache flow as other materials.
