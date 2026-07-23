# Luau Language Server (VS Code IntelliSense)

This project ships editor config so `.luau` game scripts get full IntelliSense — autocomplete,
hover docs, go-to-definition, and type checking — for the engine's scripting API (`tbx.*`, the
`toy` handle, the enums). It uses [luau-lsp](https://github.com/JohnnyMorganz/luau-lsp), the
standalone Luau language server (not the Roblox tooling).

For the API itself, see [LuauScriptingApi.md](LuauScriptingApi.md).

## What's in the repo

| File | Role |
|---|---|
| `toybox.d.luau` (repo root) | Type definitions for the whole scripting API — hand-written to mirror `engine/src/scripting/luau/luau_bindings.cpp`. This is what makes `tbx.*`, `toy`, and the enums known to the editor. Committed so everyone shares it. |
| `.luaurc` (repo root) | Nonstrict language mode + a couple of lints for the scripts. Committed. |

`.vscode/` is **gitignored** in this project, so the luau-lsp *setting* that loads the definitions
lives in your own VS Code settings rather than in the repo (see below) — but the definitions file
itself is at the root so it can be committed and shared.

## Setup

1. **Open the workspace at `Engine-dead-simple/`** — the folder that contains `.git`, `.clangd`,
   and `CMakePresets.json`. Definition-file paths resolve relative to this root.

2. **Install the extension.** Command palette or:

   ```bash
   code --install-extension johnnymorganz.luau-lsp
   ```

   The extension bundles the `luau-lsp` server binary; nothing else to install.

3. **Point luau-lsp at the definitions.** Because `.vscode/` is gitignored, add the setting to
   either your **User** settings (`Ctrl+Shift+P` → "Preferences: Open User Settings (JSON)") or a
   local, untracked **workspace** `.vscode/settings.json`:

   ```jsonc
   {
     "luau-lsp.platform.type": "standard",   // this is NOT Roblox
     "luau-lsp.sourcemap.enabled": false,
     "luau-lsp.types.definitionFiles": ["toybox.d.luau"]
   }
   ```

   A relative path resolves against the open workspace root. If you keep this in **User** settings
   and also open unrelated projects, prefer an absolute path (or a local workspace
   `.vscode/settings.json`) so other projects don't report a missing `toybox.d.luau`.

4. **Reload the window** (`Ctrl+Shift+P` → "Developer: Reload Window").

## What you get

- `tbx.` → `sandbox`, `input`, `math`, `physics`, `events`, `ui`, and the enum tables.
- `tbx.input.` → `isDown`, `getGamepadAxis`, …; `tbx.Key.` → `W`, `ESCAPE`, every key.
- `toy:` → `get`, `add`, `has`, `remove`, `with`, `move`, …; `toy.` properties → `Position`,
  `Parent`, `Name`, `Alive`, …; `toy:get(UI)` is typed as `UI?`.
- **Annotate the entry-point parameter yourself** — write `function update(toy: Toy, deltaTime:
  number)`. luau-lsp does *not* infer `toy` from the `update` signature; an unannotated `toy`
  silently becomes `any` and you lose all the typing. There is no lint that forces the
  annotation, so it is a convention: always type `toy: Toy`.

## Keeping definitions in sync

`toybox.d.luau` is maintained by hand to match the Lua bindings in
`engine/src/scripting/luau/luau_bindings.cpp`. When you add or rename a binding (a new `tbx.*`
function, a toy method, an enum member), update the definitions file too — otherwise the editor
flags the new call as unknown even though it works at runtime.

## Troubleshooting

- **"Failed to read definitions file … toybox.d.luau"** — luau-lsp reads `definitionFiles` only at
  startup / config change, so it does **not** retry after you move or add the file. Make sure the
  path in the setting resolves (relative to the workspace root) to where the file actually is,
  then **reload the window**. This is the usual cause right after relocating the file.
- **No completions at all** — confirm the `johnnymorganz.luau-lsp` extension is installed and
  enabled (`Ctrl+Shift+X`), then reload. Check Output → "Luau Language Server".
- **`tbx` is an "unknown global"** — the definitions file isn't loaded; verify the setting path
  and reload.
- **Roblox types showing up** (`game`, `workspace`, …) — set `luau-lsp.platform.type` to
  `"standard"`.
- **A valid call is flagged red** — the binding is probably missing from `toybox.d.luau`; add it.
