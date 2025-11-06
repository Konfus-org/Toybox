# Toybox Agent Standards

This document summarizes the coding expectations for anyone contributing through the Codex agent workflow. Keep it handy while editing the repo.

## Formatting & Layout
- Follow the root `.clang-format` (Allman braces, Microsoft base, 4-space indents, column limit 100, namespace indentation, sorted includes). Run the formatter on any touched file or match its style manually.
- Preserve Windows-style line endings (CRLF) for all committed files.
- Keep namespaces non-empty. If you only need a translation-unit helper, prefer `static` functions or unnamed structs over opening placeholder namespaces.
- Maintain include hygiene: headers should only include what they use, and source files should provide the heavier dependencies.

## Naming & API Shape
- Choose descriptive, self-documenting names; avoid jargon, abbreviations, or `util`-style catch‑all names.
- Use `Get...`/`Set...` prefixes for accessors and mutators to keep intent explicit.
- Prefer concrete verb phrases for commands/events (e.g., `CreateWindowCommand`), mirroring the existing messaging vocabulary.

## Documentation Expectations
- Every public API (classes, structs, functions, free helpers, message types) must be documented with:
  1. **Purpose** – what the API does.
  2. **Ownership** – who owns returned or stored resources and lifetime notes (e.g., non-owning pointers, reference expectations).
  3. **Thread Safety** – whether callers can use it concurrently, and any required synchronization.
- Keep doc comments adjacent to declarations; brief inline notes are acceptable for complex implementation details.

## Language & Feature Use
- The project targets C++23, but stick to a conservative, C-like style unless a modern feature clearly improves safety or clarity. Justify advanced language constructs in code review notes.
- Avoid `noexcept` and function/variable attributes entirely.
- Favor plain old data structures and free/static helper functions when possible; resist template metaprogramming unless unavoidable.

## Memory & Handle Management
- Prefer Toybox smart handles (`Ref`, `WeakRef`, `Scope`) whenever ownership semantics are needed. Only fall back to raw pointers for non-owning references that are trivially validated elsewhere.
- When referencing engine objects without ownership, document the expectation and lifetime contract.

## Miscellaneous Practices
- Keep translation units free of unused declarations or definitions; delete stale code rather than leaving empty shells.
- Ensure commands/events and other message types have their logic defined in `.cpp` files to keep headers lightweight.
- Validate changes with the relevant tests or builds when practical, and mention what was (or was not) run in your final summary.

By aligning with these standards, we keep the Toybox Engine consistent, readable, and predictable for every contributor.
