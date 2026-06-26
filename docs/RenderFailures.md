# Render Failure Modes

The renderer never silently drops a broken renderable. When a surface's resources can't be used,
it's drawn instead with a **loud, unlit, unmistakable debug fallback** so the failure is obvious
regardless of scene lighting. There are **five** failure modes, each with a distinct visual.

The policy lives in [`RenderValidation`](../engine/src/systems/graphics/rendering_pipeline/render_validation.h)
(the `RenderFailure` enum + the colored fallback materials, the debug checker texture, the
question-mark mesh, and the unlit validation pipeline). Each per-renderable failure is detected
during `WorldView::add_renderable` and resolved through `RenderValidation::resolve`.

## The five modes

| Mode (`RenderFailure`) | Visual fallback | Triggered when | Detected in |
| --- | --- | --- | --- |
| `SHADER_COMPILE` | **Magenta** (solid) | The material's shader fails to compile / link / build a pipeline. | `add_renderable` (`cache.add_pipeline` returns none) — `world_view.cpp` |
| `MISSING_TEXTURE` | **Cyan** over the debug **checkerboard** | A bound texture handle won't load. | `pack_material` (`cache.add_texture` returns none) — `material_packing.cpp` |
| `INVALID_MATERIAL_DATA` | **Yellow** (solid) | The material's packed parameters overflow the GPU record (> `GPU_MATERIAL_PARAM_FLOAT_COUNT` = 32 floats). | `pack_material` (param float overflow) — `material_packing.cpp` |
| `MISSING_MATERIAL` | **Red** (solid) | The material asset (or a slot's material) is missing / unassigned. | `resolve_material_instance` / slot resolution — `material_packing.cpp`, `world_view.cpp` |
| `MISSING_MESH` | **Red question-mark mesh** | The model/mesh asset is missing or fails to load. | `emit_model` (model not loadable) — `world_view.cpp` |

Notes:
- `MISSING_TEXTURE` **takes precedence** over `INVALID_MATERIAL_DATA` when a material hits both
  (`pack_material` sets the texture failure last).
- All fallbacks render through one **unlit** validation pipeline (`Fallback.vert` / `Fallback.frag`),
  so they ignore scene lighting and the size/LOD screen-door fade — a broken resource always shows at
  full strength, even when small on screen.
- The fallback materials, debug checker texture, and question-mark mesh are built once and **pinned**
  in the `GpuResourceCache` (see `RenderValidation::ensure`); they're removed on teardown.

## Seeing all five (ExampleProject)

The museum's **failure wall** (the back wall, `z = 15.5` in `Assets/Worlds/Chunks/ChunkScene_0_0_0.chunk`)
has one framed panel per mode, left → right by `x`:

| `x` | Material / model | Mode | Expected visual |
| --- | --- | --- | --- |
| -22 | `MissingTexture.mat` (binds a non-existent texture) | `MISSING_TEXTURE` | cyan checkerboard |
| -11 | `InvalidData.mat` (9 color params = 36 floats) | `INVALID_MATERIAL_DATA` | yellow |
|   0 | model `999998` (no such model) | `MISSING_MESH` | red question-mark |
|  11 | `BrokenShader.mat` (references a frag that won't compile) | `SHADER_COMPILE` | magenta |
|  22 | material id `900099` (no such material) | `MISSING_MATERIAL` | red |

Each mode also logs a one-time warning (e.g. *"Texture '…' failed to load; using cyan checker
validation."*), so a quick log scan confirms all five fired.

## Adding a new failure example

1. Author a material/model that triggers the mode (e.g. bind a bad texture handle for
   `MISSING_TEXTURE`, give a material > 32 floats of params for `INVALID_MATERIAL_DATA`).
2. Point a renderer's slot at it. The renderer keeps drawing — it just shows the debug visual.
