# Toybox CodeStandard

## Core Engineering Policies

- **Scope**: Keep changes isolated and highly reusable.
- **Duplication**: Avoid redundant code patterns without building single-use helper functions.
- **Simplicity**: Prioritize the simplest, most direct solution first. Avoid over-engineering, unnecessary abstractions, or predicting future edge cases. Add complexity only when a specific problem requires it.
- **Housekeeping**: Permanently delete stale definitions instead of leaving commented placeholders.
- **Build System**: Execute exclusively via `CMakePresets.json`.

## C++ Implementation Standards

- **Language Target**: Standardize strictly on C++23 features.
- **Initialization**: Use `()` for objects; reserve `{}` for  aggregate initialization w/ empty initialization e.g.: `return {};` or designated initializers e.g.: `auto x = MyStruct {.prop_a = 1, .prop_b = 2, ...};`, never use empty init alone always combine with aggregate AKA `= {}`.
- **Lifetimes**: Enforce strict resource safety and intent: use local values or standard references for guaranteed objects, smart pointers exclusively for heap ownership, and RAII for all resources. Replace all non-owning raw pointers with std::reference_wrapper or std::optional to explicitly communicate optionality and reassignability.
- **Includes**: Depend exclusively on direct `#include` statements; do not use forward declarations, never use blanket namespace imports.
- **Namespaces**: Ban blanket `using namespace` imports.
- **Static Globals**: Prefix mutable static global variables with `g_`; static `const` and `constexpr` constants are not globals for this rule and must use `ALL_CAPS`.
- **Type Aliases**: Use `size` and `uint` from `common/typedefs.h` instead of raw `std::size_t`.
- **Nesting**: Do not nest structs or classes within other types.
- **API Leakage**: Never expose internal namespaces in public signatures, return types, or docs.
- **Anonymity**: Replace anonymous namespaces with `static` functions.
- **Internals/Detail**: DO NOT USE, don't have detail or internal namespaces, use static instead and for private structs have a 'State' that we forward declare in the class/structs public header file and define in the cpp file.
- **Lifecycle Exceptions**: Omit Doxygen summaries entirely for `attach`, `detach`, `update`, `on_attach`, `on_detach`, `on_update`, and `on_fixed_update`.

## Testing & Verification

- **Behavior-Driven Testing**: Write unit tests for all new features. Focus strictly on testing behavioral outcomes, never implementation details or default values.
- **Dual-Scenario Coverage**: Every behavior requires exactly two explicit test cases: a positive test verifying success under correct conditions, and a negative test verifying graceful failure under invalid conditions.
- **AAA Pattern**: Enforce the Arrange-Act-Assert structure cleanly inside every test function.
- **Strict Isolation**: Ban all filesystem and network I/O. Force the use of mocks, fakes, or stubs for all external dependencies.
- **Target Verification**: The System Under Test (SUT) must be actually instantiated and executed. Never mock the class or function you are trying to test.
- **Build System**: Execute builds exclusively via `CMakePresets.json`.
- **Always Build w/ Sanitizers**: Normal Debug presets are intentionally unsanitized for framerate. For ASan+UBSan startup/testing validation, use: `cmake --preset clang-sanitize-tests` -> `cmake --build --preset clang-sanitize-debug-tests` -> `ctest --preset test-clang-sanitize-debug`.
- **Always Test Changes**: Launch the 3d Example in normal debug mode for interactive startup/shutdown. Also use the Clang sanitizer mode when validating memory safety, undefined behavior, or agent startup/test confidence.

## Documentation

- Insert clear, concise code comments that explain the "why" rather than the "what." Focus strictly on documenting underlying assumptions, complex business logic, constraints, and non-obvious algorithmic decisions. Avoid commenting on self-explanatory, idiomatic code.
- Use Doxygen `///` style summaries.
- Summaries are required on:
  - `struct` declarations.
  - `class` declarations.
  - Public methods.
- Keep Doxygen summaries directly adjacent to their declaration (no blank line between summary and declaration).
- Plugin and example lifecycle methods (`attach`, `detach`, `update`, including `on_attach`, `on_detach`, `on_update`, `on_fixed_update`) do not require Doxygen summaries.

## File Layout

Strictly follow the below file layout:

```cpp
#pragma once // do not use old style ifdefs
#includes... // <> for external, "" for internal, should be sorted by name.

// Header-file:
namespace tbx
{
    Constants...
    Usings...
    Structs...
    Classes...
    Methods (sort by keyword: static/inline/etc, then by name)...
}

// Source-file:
namespace tbx
{
    // Internal constants, using, structs and classes should always be at the top
    
    Constants A...
    Usings A...
    Structs A...
    Classes A...

    //// CATEGORY A //// <-- use these to break up source files into categories and make them easy to find things

    static Methods (sort by keyword: static/inline/etc, then by name)...
{
    //// CATEGORY B ////

    static Methods (sort by keyword: static/inline/etc, then by name)...
    
    //// MY COOL CLASS ////

    Mirror header file here...
    
    //// MY OTHER COOL CLASS ////
    
    Mirror header file here...
}
```

## Class / Struct Layout

Strictly follow this layout format for every struct/class block:

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
