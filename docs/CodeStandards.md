# Toybox CodeStandard

## Core Engineering Policies

- **Scope**: Keep changes isolated and highly reusable.
- **Mechanisms**: Prefer existing engine utilities over writing raw custom solutions.
- **Duplication**: Avoid redundant code patterns without building single-use helper functions.
- **Simple**: Prioritize the simplest, most direct solution first. Avoid over-engineering, unnecessary abstractions, or predicting future edge cases. Add complexity only when a -pecific problem requires it.
- **Housekeeping**: Permanently delete stale definitions instead of leaving commented placeholders.
- **Comments**: Insert clear, concise code comments that explain the "why" rather than the "what." Focus strictly on documenting underlying assumptions, complex business logic, constraints, and non-obvious algorithmic decisions. Avoid commenting on self-explanatory, idiomatic code.
- **Testing**: All new features should have associated unit tests. Behavior-Driven Testing: Write unit tests for all new features. Focus strictly on testing behavioral outcomes, never implementation details or default values. Dual-Scenario Coverage: Every behavior requires two explicit test cases: a positive test verifying success under correct conditions, and a negative test verifying graceful failure under invalid conditions. AAA Pattern: Enforce the Arrange-Act-Assert structure cleanly inside every test function.Strict Isolation: Ban all filesystem and network I/O. Force the use of mocks, fakes, or stubs for all external dependencies.
- **Build System**: Execute exclusively via `CMakePresets.json`.
- **Clang Toolchain**: `cmake --preset clang` -> `cmake --build --preset clang-debug` -> `ctest --preset test-clang-debug`
- **MSVC Toolchain**: `cmake --preset msvc` -> `cmake --build --preset msvc-debug` -> `ctest --preset test-msvc-debug`
- **Always Test Changes**: Launch the 3d Example in debug mode and ensure there are no exceptions/errors/warnings on launch and shutdown.

## C++ Implementation Standards

- **Language Target**: Standardize strictly on C++23 features.
- **Constructors**: Never use the `explicit` keyword on constructors.
- **Initialization**: Use `()` for objects; reserve `{}` for empty initialization or designated initializers.
- **Lifetimes**: Enforce strict resource safety and intent: use local values or standard references for guaranteed objects, smart pointers exclusively for heap ownership, and RAII for all resources. Replace all non-owning raw pointers with std::reference_wrapper or std::optional to explicitly communicate optionality and reassignability.
- **Includes**: Depend exclusively on direct `#include` statements; do not use forward declarations, never use blanket namespace imports.
- **Namespaces**: Ban blanket `using namespace` imports.
- **Type Aliases**: Use `size` and `uint` from `common/typedefs.h` instead of raw `std::size_t`.
- **Nesting**: Do not nest structs or classes within other types.
- **API Leakage**: Never expose internal namespaces in public signatures, return types, or docs.
- **Anonymity**: Replace anonymous namespaces with `static` functions.
- **Internals/Detail**: DO NOT USE, don't have detail or internal namespaces, use static instead and for private structs have a 'State' that we forward declare in the class/structs public header file and define in the cpp file.
- **Lifecycle Exceptions**: Omit Doxygen summaries entirely for `attach`, `detach`, `update`, `on_attach`, `on_detach`, `on_update`, and `on_fixed_update`.

## Documentation

- Use Doxygen `///` summaries only for:
  - `struct` declarations.
  - `class` declarations.
  - Public methods.
- Simple properties may use `//` comments when helpful.
- Keep Doxygen summaries directly adjacent to their declaration (no blank line between summary and declaration).
- Plugin and example lifecycle methods (`attach`, `detach`, `update`, including `on_attach`, `on_detach`, `on_update`, `on_fixed_update`) do not require Doxygen summaries.

## File Layout:

``` cpp
#pragma once // do not use old style ifdefs
#includes... // <> for external, "" for internal, should be sorted by name. If order matters then wrap in // clang-format off ... // clang-format on comments

// Source-file support declarations and definitions should live in the associated .cpp file above
// the public declarations. Header-only support code is public API support.
namespace tbx
{
    Usings...
    Methods (should always be static when local to the source file)
    Structs...
    Classes...

    // the public API
    Usings...
    Methods (sort by keyword: static/inline/etc, then by name)...
    Structs...
    Classes...
}
```

## Class / Struct Layout
Use this ordering for every class:

```cpp
class Name :
    InheritFromA,
    InheritFromB
{
  public:
    Usings...

  public:
    Constructor
    Destructor

  public:
    CopyConstructors...
    AssignmentOperators...

  public:
    Methods (sort by keyword: static/inline/etc, then by name)

  public:
    Properties (sort by keyword: static/inline/etc, then by name)

  private:
    Constructor
    Destructor

  private:
    CopyConstructors...
    AssignmentOperators...

  private:
    Methods (sort by keyword: static/inline/etc, then by name)

  private:
    Properties (sort by keyword: static/inline/etc, then by name)
};
```

Structs should remain simple and if everything is public they are exempt from the above layout, if a struct has private members then it should follow the same format as a class. Structs should be plain ol' data with little or no behavior

Struct with all public members:
```cpp
struct Name
{
  Usings
  Constructor
  Destructor
  Properties...
}
```

## Type Organization
- Do not nest public classes or structs inside other classes/structs.
- Private class-owned implementation details may use nested forward declarations when the definitions live in the owning `.cpp` file or are required for header-only template code.
- Move helper types that are not owned by a class to top-level declarations within the same namespace.

## Formatting
- Follow root `.clang-format`.
- Use LF line endings.
- Keep `#include` directives contiguous.
- Prefer simple, flat control flow and remove unnecessary nesting.

## Graphics Pipeline
Refer to Shader Pipeline Docs [here](ShaderPipeline.md)
