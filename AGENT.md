# Toybox Agent Standards

## Language & Feature Use
- The project targets C++23.
- Prefer copy-style initialization (`int value = {};`, `auto widget = Widget(args);`) over brace-only or direct-call forms (`int value{};`, `auto widget(Widget(args));`) to keep intent obvious.
- Avoid forward declarations where possible, prefer directly including what is needed.

## Formatting & Layout
- Follow the root `.clang-format` (Allman braces, Microsoft base, 4-space indents, column limit 100, namespace indentation, sorted includes). Run the formatter on any touched file or match its style manually.
- Use windows style line endings: `CRLF`.
- Keep `#include` directives contiguous—no blank lines between include statements.
- Keep namespaces non-empty. If you only need a translation-unit helper, prefer `static` functions or unnamed structs empty over empty or detail namespaces.
- Maintain include hygiene: headers should only include what they use, and source files should provide the heavier dependencies.
- Don't use forward declarations where possible, directly include what is needed.
- Simplify and remove nesting where possible.
- Don't nest structs/classes and don't make massive header files, break things up so the project stays organized and things are easy to find.
- Prefer to not use curly braces for single line ifs, for, and while loops.
- Prefer designated initialization when available.

## Naming & API Shape
- Choose descriptive, self-documenting names; avoid jargon, abbreviations, or `util`-style catch‑all names.
- Use `get...`/`set...` prefixes for accessors and mutators to keep intent explicit and question style names for bool returns.
- methods that have an optional out with a bool return should have a `try` prefix.
- Prefer concrete verb phrases for commands/events (e.g., `CreateWindowCommand`), mirroring the existing messaging vocabulary.

## Documentation Expectations
- Every public API (classes, structs, functions, free helpers, message types) must be documented with:
  1. **Purpose** – what the API does.
  2. **Ownership** – who owns returned or stored resources and lifetime notes (e.g., non-owning pointers, reference expectations).
  3. **Thread Safety** – whether callers can use it concurrently, and any required synchronization.
- Keep doc comments adjacent to declarations; brief inline notes are acceptable for complex implementation details.
- Summaries should be in microsoft xml format.
- Complex and long methods should be broken up with comments explaining things, and preferably if really large, they should be broken up into sub methods that are then documented with summaries and meaningful names.

## Memory & Handle Management
- Prefer smart pointers whenever ownership semantics are needed. Only fall back to raw pointers for non-owning references that are trivially validated elsewhere.
- When referencing engine objects without ownership, document the expectation and lifetime contract.

## Miscellaneous Practices
- Keep translation units free of unused declarations or definitions; delete stale code rather than leaving empty shells.
- Ensure commands/events and other message types have their logic defined in `.cpp` files to keep headers lightweight.
- Validate changes with the relevant tests or builds when practical, and mention what was (or was not) run in your final summary.

By aligning with these standards, we keep the Toybox Engine consistent, readable, and predictable for every contributor.
