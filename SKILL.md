---
name: toybox
description: Use when the user says Toybox, calls the Toybox plugin, or asks for work in C:\Users\jercl\Projects\Toybox\Engine. Enforce Toybox AGENTS.md and docs policies; start every change with a plan; verify changes before completion; use all bundled C++ skills for C++ work.
---

# Toybox

Use this as the entry point for Toybox work. When the user says "Toybox" or asks for Toybox Engine work, apply the repository policies first, then route to the relevant bundled skills.

## Always Check

For Toybox Engine work, treat these files as authoritative:
- `C:\Users\jercl\Projects\Toybox\Engine\AGENTS.md`
- `C:\Users\jercl\Projects\Toybox\Engine\docs\CodeStandards.md`
- `C:\Users\jercl\Projects\Toybox\Engine\docs\Contributing.md`
- `C:\Users\jercl\Projects\Toybox\Engine\docs\ShaderPipeline.md`

Read the relevant file sections when they are available. The rules below are the operating baseline.

## Core Policies

- Keep changes focused and reusable; prefer existing engine utilities over new mechanisms.
- Follow DRY, but do not create throwaway helpers for one-off logic.
- Prefer references for guaranteed objects, smart pointers otherwise, and RAII for resource lifetime.
- Prefer direct includes over forward declarations.
- Remove stale declarations/definitions instead of leaving placeholders.
- Document assumptions when they affect behavior.
- Start every code, config, docs, build, test, or plugin change with a brief plan before editing.
- For every change, verify the actual result before reporting completion.
- Unit tests must use Arrange / Act / Assert and must not use filesystem or network I/O; use mocks, fakes, or stubs for those boundaries.
- Build and test through `CMakePresets.json`: Clang uses `cmake --preset clang`, `cmake --build --preset clang-debug`, and `ctest --preset test-clang-debug`; MSVC uses `cmake --preset msvc`, `cmake --build --preset msvc-debug`, and `ctest --preset test-msvc-debug`.
- Run clang-format and clang-tidy for C++ changes when available.

## C++ Rule

For any C++ implementation, refactor, debug, review, build break, or test change, ALWAYS use all C++ related skills together:
- `cpp-pro`
- `cpp-coding-standards`
- `memory-safety-patterns`

Do not choose only one of these for C++ work. Add domain-specific skills on top as needed.

Toybox C++ standards include:
- Target C++23.
- Do not use C++ attributes such as `[[nodiscard]]`.
- Do not use `explicit` on constructors.
- Prefer `()` initialization for structs/classes; use `{}` only for simple empty initialization or designated-style readability.
- Do not use blanket namespace imports.
- Prefer Toybox aliases such as `size` and `uint` from `common/typedefs.h` instead of `std::size_t` or raw uint spellings when appropriate.
- Do not use anonymous namespaces; prefer `static` functions in an internal namespace and internal implementation files.
- Do not nest structs or classes.
- Do not expose internal namespaces in public APIs, return types, or public documentation.
- Keep internal declarations/definitions in matching `*_internal.h` and `*_internal.cpp` files under the owning `internal` folder when adding internal surface.
- Use Doxygen `///` only for structs, classes, and public methods; do not add Doxygen to private members.
- Lifecycle methods such as `attach`, `detach`, `update`, `on_attach`, `on_detach`, `on_update`, and `on_fixed_update` do not require Doxygen summaries.

## Shader Rule

- Follow `docs/ShaderPipeline.md` for shader shape and ownership.
- Base shader files provide assumptions, material files provide ABI, lighting files provide algorithms, and user shaders provide `main()`.
- Material, lighting, geometry, and other non-post-process shaders output linear scene color. Exposure, tone mapping, gamma correction, and display conversion belong in explicit post-processing passes.
- Use Toybox shader naming conventions: PascalCase files/types, uppercase snake case constants/macros, `u_` uniforms, `a_` attributes, `v_` varyings, and `o_` outputs.
- Keep shader bases small, avoid giant conditional compilation, split by stage/domain, and keep shadow shaders simple.
- Put resource-backed shader assets under `resources/Shaders/...`; add matching `.meta` files for new built-in resources.

## Routing

- C++ work: always use `cpp-pro`, `cpp-coding-standards`, `memory-safety-patterns`, and `context-tools`.
- OpenGL, rendering, graphics API, or shader work: add `graphics-api-hooking` and `shader-programming-glsl`.
- 3D gameplay, camera, physics, or scene work: add `3d-games`.

Work in `C:\Users\jercl\Projects\Toybox\Engine` by default when no Toybox checkout is specified.
