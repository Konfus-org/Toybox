# Toybox Agent Guide

This file defines contributor workflow rules for agents working in this repository.

## Primary coding standard
- Follow `CODE_STANDARDS.md` for all C++ style, formatting, class layout, and documentation expectations.

## Agent rules
- Act as a senior C++ engineer with game development expertise.
- Follow DRY principles; avoid duplicated logic and duplicated data transformations.
- Avoid raw pointers when possible; use references where objects are guaranteed to exist and smart pointers in most other situations.
- Use RAII consistently so resources are cleaned up when an object is destroyed.
- Do not remake the wheel; reuse existing engine utilities/components before introducing new implementations.
- When it makes sense, design features and helpers for reusability.
- Avoid throwaway helper methods. If a function will not be reused, implement it inline and use comments to break up complex logic when that improves readability.
- DO NOT use anonymous namespaces, prefer private or static in a detail namespace over anonymous.
- structs and classes should not be nested, it hurts readability. If its 'private' just put them in a source file and wrap into a detail namespace.
- Comment on and document assumptions.
- Keep changes focused and minimal to the requested scope.
- Prefer direct includes over forward declarations.
- Remove stale/unused declarations and definitions instead of leaving placeholders.
- Unit tests must not use filesystem or network I/O.
- Unit tests must use mocks/fakes/stubs for all filesystem and network behavior.
- Use Arrange / Act / Assert structure for unit tests.
- Test changes by building with the presets in `CMakePresets.json`.
- Use `cmake --preset clang` followed by `cmake --build --preset clang-debug` on macOS/Linux or when using Clang.
- Use `cmake --preset msvc` followed by `cmake --build --preset msvc-debug` on Windows when using the MSVC toolchain.
- Run the matching `ctest` preset (`test-clang-debug` or `test-msvc-debug`) when tests are affected or available.
