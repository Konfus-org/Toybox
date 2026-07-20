# Toybox CodeStandard

## Core Engineering Policies

- **Scope**: Keep changes isolated and highly reusable.
- **Duplication**: Avoid redundant code patterns without building single-use helper functions.
- **Simplicity**: Prioritize the simplest, most direct solution first. Avoid over-engineering, unnecessary abstractions, or predicting future edge cases. Add complexity only when a specific problem requires it. Prefer simple, flat control flow and remove unnecessary nesting. Do not have tons of small methods! Only break things apart when we genuinely need to re-use something or if the method is gargantuan.
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
- **Accessor Naming**: Getters use a `get_` prefix and setters a `set_` prefix — never bare-noun accessors (`get_width()`, not `width()`).
- **Bool Naming**: Bool-returning methods and bool members use `is_` or another question-style prefix that reads naturally (`is_headless()`, `is_down`); never omit the prefix.
- **Verbosity**: No shorthand names — verbose and descriptive wins (`register_type` not `reg`, `initialize` not `init`, `delta_time` not `dt`).
- **Data-Oriented Modules**: Where a subsystem is plain state + queries, prefer a short namespace of free functions with state as statics in the module's `.cpp` (`tbx::input::is_down(key)`, `tbx::gpu::draw(...)`, `tbx::files::read_text(...)`) — no manager class ceremony. Keep classes where RAII genuinely earns it: resource owners with real teardown/ordering (`Engine`, `Jobs`, `Window`, `Sandbox`) and small data/handle types (`Task`, `Signal`, `Toy`, `Uuid`).
- **RAII Over Create/Destroy**: Never expose create/destroy function pairs — creation returns an owning smart pointer (or value RAII type) whose destructor releases the resource (`Result<std::unique_ptr<gpu::Shader>>`, never `destroy_shader`).
- **No Raw/Void Pointers**: Beyond the existing lifetimes rule, replace `void*` with modern alternatives — `std::span<std::byte>`/`std::byte*` for type-erased memory, `std::reference_wrapper`/`std::optional` for references; raw pointers only at true C boundaries (Lua userdata payloads), commented as such.
- **Const By Default**: Locals, parameters, and methods are `const` unless mutation is the point.
- **Third-Party Seams**: Every third-party library sits behind exactly one engine-owned boundary: compiled backends behind `tbx_backend()` folders (sdl/gl/jolt/luau), header-only libs behind one wrapper header (`core/math.h` = glm, `core/json.h` = nlohmann, `core/log.h` = spdlog, `ecs/registry.h` = entt). Nothing else includes a third-party header directly — swapping a lib touches its one seam.

## Writing Unit Tests

- **Behavior-Driven Testing**: Write unit tests for all new features. Focus strictly on testing behavioral outcomes, never implementation details or default values.
- **Dual-Scenario Coverage**: Every behavior requires exactly two explicit test cases: a positive test verifying success under correct conditions, and a negative test verifying graceful failure under invalid conditions.
- **AAA Pattern**: Enforce the Arrange-Act-Assert structure cleanly inside every test function.
- **Strict Isolation**: Ban all filesystem and network I/O. Force the use of mocks, fakes, or stubs for all external dependencies.
- **Target Verification**: The System Under Test (SUT) must be actually instantiated and executed. Never mock the class or function you are trying to test.

# Verifying Changes

- **Build w/ Sanitizers**: Normal debug presets are intentionally unsanitized for framerate. For ASan+UBSan startup/testing validation, use: `cmake --preset clang-sanitize-tests` -> `cmake --build --preset clang-sanitize-debug-tests` -> `ctest --preset test-clang-sanitize-debug`.
- **Launch Example**: Launch the example app in normal debug mode for interactive startup/shutdown. Also use the Clang sanitizer mode when validating memory safety, undefined behavior, or agent startup/test confidence.

## Documentation

- Insert clear, concise code comments that explain the "why" rather than the "what." Focus strictly on documenting underlying assumptions, complex business logic, constraints, and non-obvious algorithmic decisions. Avoid commenting on self-explanatory, idiomatic code.
- Use Doxygen `///` style summaries.
- Summaries are required on:
  - `struct` declarations.
  - `class` declarations.
  - Public methods.
- Keep Doxygen summaries directly adjacent to their declaration (no blank line between summary and declaration).

## File Layout & Formatting

- Follow root `.clang-format`.
- Use LF line endings.
- Keep `#include` directives contiguous.

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
