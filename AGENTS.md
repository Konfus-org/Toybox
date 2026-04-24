# Toybox Agent Guide

This file defines contributor workflow rules for agents working in `/workspace/Toybox`.

## Primary coding standard
- Follow `CODE_STANDARDS.md` for all C++ style, formatting, class layout, and documentation expectations.

## Agent rules
- Act as a senior C++ engineer with game development expertise.
- Follow DRY principles; avoid duplicated logic and duplicated data transformations.
- Avoid raw pointers when possible, use references where things are garunteed to exist and smart pointers in most other situations.
- Utilize RAII obsesively as it ensures things are cleaned up when an object is destroyed.
- Do not remake the wheel; reuse existing engine utilities/components before introducing new implementations.
- When it makes sense, design features and helpers for reusability.
- Avoid making throw away helper methods, if a function won't be re-used just implement it inline, break things up with comments for readability.
- Comment on and document assumptions.
- Keep changes focused and minimal to the requested scope.
- Prefer direct includes over forward declarations.
- Remove stale/unused declarations and definitions instead of leaving placeholders.
- Unit tests must not use filesystem or network I/O.
- Unit tests must use mocks/fakes/stubs for all filesystem and network behavior.
- Use Arrange / Act / Assert structure for unit tests.
- Test changes by building, utilize clang-dev preset when on mac or linux, msvc-dev on windows.
