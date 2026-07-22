# Render Failure Modes

The renderer never silently drops a broken renderable. When a surface's resources can't be used,
it's drawn instead with a **loud, unlit, unmistakable debug fallback** so the failure is obvious
regardless of scene lighting. There are **five** failure modes, each with a distinct visual.

The policy lives in the renderer ([`renderer.cpp`](../engine/src/gfx/renderer.cpp): the
`RenderFailure` enum + the fallback colors, the debug checker texture, the question-mark mesh, and
the unlit validation pipeline [`fallback.vert`](../resources/Shaders/Tbx/fallback.vert) /
[`fallback.frag`](../resources/Shaders/Tbx/fallback.frag)). Each per-draw failure is detected
during asset resolution and drawn by the geometry pass; the log says why exactly once per asset.

## The five modes

| Mode (`RenderFailure`) | Visual fallback | Triggered when | Detected in |
| --- | --- | --- | --- |
| `SHADER_COMPILE` | **Magenta** (solid) | The material's shader fails to load / compile / link. | `resolve_material_shaders` — `renderer.cpp` |
| `MISSING_TEXTURE` | **Cyan** over the debug **checkerboard** | A bound texture handle won't load. | `resolve_texture_handle` (via `resolve_surface`) — `renderer.cpp` |
| `INVALID_MATERIAL_DATA` | **Yellow** (solid) | The material's uniforms bag is not a JSON object, so it cannot be applied. | `resolve_surface` — `renderer.cpp` |
| `MISSING_MATERIAL` | **Red** (solid) | The material asset is missing / won't load. | `resolve_surface` — `renderer.cpp` |
| `MISSING_MESH` | **Red question-mark mesh** | The model asset is missing or fails to load. | `resolve_mesh` — `renderer.cpp` |

Notes:
- `MISSING_TEXTURE` **takes precedence** over `INVALID_MATERIAL_DATA` when a material hits both
  (the texture failure is set last in `resolve_surface`).
- All fallbacks render through one **unlit** validation pipeline (`fallback.vert` / `fallback.frag`),
  so they ignore scene lighting — a broken resource always shows at full strength.
- The fallback shader, debug checker texture, and question-mark mesh are built once in
  `ensure_renderer_ready` and owned by `RendererState` — they die with the runtime, before the GL
  context.
