# Toybox Agent Guide

This file defines contributor workflow rules for agents working in this repository.

## Reference Files

Strictly follow the `docs/CodeStandards.md` and keep `docs` up-to-date when making sweeping changes to architecture.

## Core Engineering Policies

- **Scope**: Keep changes isolated and highly reusable.
- **Mechanisms**: Prefer existing engine utilities over writing raw custom solutions.
- **Duplication**: Avoid redundant code patterns without building single-use helper functions.
- **Simplicity**: Prioritize the simplest, most direct solution first. Avoid over-engineering, unnecessary abstractions, or predicting future edge cases. Add complexity only when a specific problem requires it.
- **Housekeeping**: Permanently delete stale definitions instead of leaving commented placeholders.
- **Comments**: Insert clear, concise code comments that explain the "why" rather than the "what." Focus strictly on documenting underlying assumptions, complex business logic, constraints, and non-obvious algorithmic decisions. Avoid commenting on self-explanatory, idiomatic code.

## Testing & Verification

- **Always Build Tests w/ Sanitizers**: Use: `cmake --preset clang-sanitize-tests` and fix any issues raised by sanitizers or test failures.
- **Always Test Changes**: Build with  `ctest --preset clang-debug` and launch the example app under `examples` and test startup/shutdown. Ensure the app fully starts up, runs for a few seconds, then shut it down and examine logs under the build dirs 'logs' folder to ensure their are no warnings or errors logged, if there are fix them and re-test until no warnings/errors.
- **Rendering Verification**: When validating rendering changes, launch the existing `ThreeDExampleLauncher.exe`, bring the window to the foreground, and capture a desktop screenshot once the Toybox window is visible. Use the screenshot as a visual regression check that the frame rendered as expected.
- **Performance Profiling**: When asked to profile or debug performance issues, run the VSDiagnostics.exe against the `examples/3d_example` launcher. Capture actionable CPU/GPU/frame-time evidence before proposing fixes, then verify the same scenario again after changes.

## C++ Implementation Standards

- **Language Target**: Standardize strictly on C++23 features.
- **Constructors**: Mark all single-argument constructors as `explicit` to prevent implicit conversions, unless explicitly requested otherwise.
- **Initialization**: Use `()` for objects; reserve `{}` for empty initialization or designated initializers.
- **Lifetimes**: Enforce strict resource safety and intent: use local values or standard references for guaranteed objects, smart pointers exclusively for heap ownership, and RAII for all resources. Replace all non-owning raw pointers with `std::reference_wrapper` or `std::optional` to explicitly communicate optionality and reassignability.
- **Casting**: Use c++ style casting but NEVER cast to void. If we don't use a return that is fine.
- **Includes**: Depend exclusively on direct `#include` statements. Do not use blanket namespace imports. Wrap multi-line sorted blocks in `// clang-format off` / `// clang-format on` if order matters.
- **Forward Declarations**: Minimize forward declarations. They are permitted exclusively for forward-declaring private structural `State` definitions inside public headers.
- **Namespaces**: Ban blanket `using namespace` imports.
- **Static Globals**: Prefix mutable static global variables with `g_`. Static `const` and `constexpr` constants are not globals for this rule and must use `ALL_CAPS`.
- **Type Aliases**: Use `size` and `uint` from `common/typedefs.h` instead of raw `std::size_t`.
- **Nesting**: Do not nest structs or classes within other types.
- **API Leakage**: Never expose internal namespaces in public signatures, return types, or docs.
- **Anonymity**: Replace anonymous namespaces with `static` functions.
- **Internals/Detail**: Do not create or use `.detail` or `.internal` namespaces. Use `static` file-scope modifiers instead. For private implementations, forward-declare a `State` struct in the class public header and define it fully inside the `.cpp` file.
- **Lifecycle Exceptions**: Omit Doxygen summaries entirely for `attach`, `detach`, `update`, `on_attach`, `on_detach`, `on_update`, and `on_fixed_update`.
