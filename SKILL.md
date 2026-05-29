---
name: toybox
description: Activates when the user mentions "Toybox" or requests changes within the Toybox Game Engine. Enforces repository policies and initializes tasks by executing the planning mode.
---

# Toybox Engine Rulebook

Use this skill as the mandatory entry point for all Toybox Engine development. Always run the plan mode: `/plan` to validate direction before modifying code.

## Authoritative Reference Files

Treat these local files as the absolute source of truth for the codebase. Read relevant sections prior to starting execution:
* `\AGENTS.md`
* `\docs\CodeStandards.md`
* `\docs\Contributing.md`
* `\docs\ShaderPipeline.md`

## Routing Matrix

Dynamically bundle these precise sub-skills based on task domain:
* **All C++ Tasks**: Mandatorily bundle `cpp-pro`, `cpp-coding-standards`, `memory-safety-patterns`, and `context-tools`.
* **Rendering & Shaders**: Add `graphics-api-hooking` and `shader-programming-glsl`.
* **Gameplay & Simulation**: Add `3d-games` for cameras, physics, and scene graphs.
* **Fallback Directory**: Default to `.\` root project dir if no path is explicitly provided.

## Core Engineering Policies

* **Scope**: Keep changes isolated and highly reusable.
* **Mechanisms**: Prefer existing engine utilities over writing raw custom solutions.
* **Duplication**: Avoid redundant code patterns without building single-use helper functions.
* **Lifetimes**: Enforce strict resource safety and intent: use local values or standard references for guaranteed objects, smart pointers exclusively for heap ownership, and RAII for all resources. Replace all non-owning raw pointers with std::reference_wrapper or std::optional to explicitly communicate optionality and reassignability.
* **Includes**: Depend exclusively on direct `#include` statements; do not use forward declarations.
* **Housekeeping**: Permanently delete stale definitions instead of leaving commented placeholders.
* **Comments**: Insert clear, concise code comments that explain the "why" rather than the "what." Focus strictly on documenting underlying assumptions, complex business logic, constraints, and non-obvious algorithmic decisions. Avoid commenting on self-explanatory, idiomatic code.
* **Testing Boundary**: Enforce Arrange/Act/Assert patterns.
* **Testing Isolation**: Ban filesystem and network I/O in tests using mocks, fakes, or stubs.
* **Build System**: Execute exclusively via `CMakePresets.json`.
* **Clang Toolchain**: `cmake --preset clang` -> `cmake --build --preset clang-debug` -> `ctest --preset test-clang-debug`
* **MSVC Toolchain**: `cmake --preset msvc` -> `cmake --build --preset msvc-debug` -> `ctest --preset test-msvc-debug`
* **Always Test Changes**: Launch the 3d Example in debug mode and ensure there are no exceptions/errors/warnings on launch and shutdown.

## C++ Implementation Standards

* **Language Target**: Standardize strictly on C++23 features.
* **Constructors**: Never use the `explicit` keyword on constructors.
* **Initialization**: Use `()` for objects; reserve `{}` for empty initialization or designated initializers.
* **Namespaces**: Ban blanket `using namespace` imports.
* **Type Aliases**: Use `size` and `uint` from `common/typedefs.h` instead of raw `std::size_t`.
* **Anonymity**: Replace anonymous namespaces with `static` functions inside an `internal` namespace.
* **Nesting**: Do not nest structs or classes within other types.
* **API Leakage**: Never expose internal namespaces in public signatures, return types, or docs.
* **Internal Layout**: Place internal code in `*_internal.h` and `*_internal.cpp` within the component's `internal/` folder.
* **Documentation**: Apply Doxygen `///` only to public interfaces; ban them on private members.
* **Lifecycle Exceptions**: Omit Doxygen summaries entirely for `attach`, `detach`, `update`, `on_attach`, `on_detach`, `on_update`, and `on_fixed_update`.