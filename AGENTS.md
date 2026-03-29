# Toybox Agent Guide

This file defines contributor workflow rules for agents working in `/workspace/Toybox`.

## Primary coding standard
- Follow `CodeStandard.md` for all C++ style, formatting, class layout, and documentation expectations.

## Agent workflow
- Follow DRY principles; avoid duplicated logic and duplicated data transformations.
- Do not remake the wheel; reuse existing engine utilities/components before introducing new implementations.
- When it makes sense, design features and helpers for reusability.
- Keep changes focused and minimal to the requested scope.
- Prefer direct includes over forward declarations.
- Do not use `using namespace ...`.
- Remove stale/unused declarations and definitions instead of leaving placeholders.
- Keep message/command logic in `.cpp` files when applicable.
- Run relevant tests/build checks for touched areas when practical.
- In final summaries, list what was validated and call out any gaps.

## Testing guidance
- Tests must not use filesystem or network I/O.
- Tests must use mocks/fakes/stubs for all filesystem and network behavior.
- Use Arrange / Act / Assert structure.
- Add concise comments only when they clarify non-obvious behavior.
