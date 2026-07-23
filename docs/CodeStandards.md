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
- **Folder = Namespace**: Everything in a module folder lives in that folder's namespace (`ecs/` -> `tbx::ecs`, `gfx/` -> `tbx::gfx`, `assets/` -> `tbx::assets`, ...) or the module's established one (`platform/` -> `tbx::windows` + `tbx::input`, `scripting/` -> `tbx::scripts`, `assets/builtin.h` -> `tbx::builtin`). Only root-level headers (`app.h`, `runtime.h`) and the foundations in `math/` and `utils/` (`Vec3`, `Result`, `Uuid`, `Color`, ...) stay plain `tbx::` — the vocabulary every module shares.
- **Static Globals**: Prefix mutable static global variables with `g_`; static `const` and `constexpr` constants are not globals for this rule and must use `ALL_CAPS`.
- **Type Aliases**: Use `size` and `uint` from `common/typedefs.h` instead of raw `std::size_t`.
- **Nesting**: Do not nest structs or classes within other types.
- **API Leakage**: Never expose internal namespaces in public signatures, return types, or docs.
- **Accessor Naming**: Getters use a `get_` prefix and setters a `set_` prefix — never bare-noun accessors (`get_width()`, not `width()`).
- **Fluent Mutators**: Mutating methods on ECS handles (`ecs::Toy`) and the builtin data blocks/assets (`Transform`, `Material`, `RigidBody`, `Camera`, `Ui`, …) return a reference to the object they mutated (`set_name` returns `Toy&`, `set_roughness` returns `Material&`) so a toy or block builds in one chain — `spawn("Grunt").with(Transform{}).sticker("enemy").set_enabled(true)`, `Material{}.set_albedo(red).set_roughness(0.3f)`. Read accessors (`get_`/`is_`) still return their value. The Lua bindings mirror this: `toy:with("Transform", {...}):sticker("enemy"):set_enabled(true)`.
- **Bool Naming**: Bool-returning methods and bool members use `is_` or another question-style prefix that reads naturally (`is_headless()`, `is_down`); never omit the prefix.
- **Verbosity**: No shorthand names — verbose and descriptive wins (`register_type` not `reg`, `initialize` not `init`, `delta_time` not `dt`).
- **Data-Oriented Modules**: Where a subsystem is plain state + queries, prefer a short namespace of free functions with state as statics in the module's `.cpp` (`tbx::input::is_down(key)`, `tbx::gfx::draw(...)`, `tbx::files::read_text(...)`) — no manager class ceremony. Keep classes where RAII genuinely earns it: resource owners with real teardown/ordering (`Engine`, `Jobs`, `Window`, `Sandbox`) and small data/handle types (`Task`, `Signal`, `Toy`, `Uuid`).
- **Module Ownership**: A setting belongs to the module that means it, not the surface that applies it — vsync is the gfx module's request (`gfx::set_vsync`/`gfx::is_vsync_enabled`); the platform backend reads it and applies the swap interval per window surface.
- **RAII Over Create/Destroy**: Never expose create/destroy function pairs — creation returns an owning smart pointer (or value RAII type) whose destructor releases the resource (`Result<std::unique_ptr<gfx::Shader>>`, never `destroy_shader`).
- **No Raw/Void Pointers**: Beyond the existing lifetimes rule, replace `void*` with modern alternatives — `std::span<std::byte>`/`std::byte*` for type-erased memory, `std::reference_wrapper`/`std::optional` for references; raw pointers only at true C boundaries (Lua userdata payloads), commented as such.
- **Const By Default**: Locals, parameters, and methods are `const` unless mutation is the point.
- **Third-Party Seams**: Every third-party library sits behind exactly one engine-owned boundary: compiled backends behind `tbx_backend()` folders (sdl/gl/jolt/luau), header-only libs behind one wrapper header (`core/math.h` = glm, `core/json.h` = nlohmann, `core/log.h` = spdlog, `ecs/registry.h` = entt). Importers are seams too: `src/gfx/model.cpp` is the assimp seam, `src/gfx/texture.cpp` the stb seam. Nothing else includes a third-party header directly — swapping a lib touches its one seam.

## Internals & the `tbx::internal` Boundary

The public surface is the free-function API and the plain data types that flow through it. Everything the
runtime drives internally — module *state* and the functions that consume it — lives in `tbx::internal` and,
where practical, in `_internal.h` headers under `src/` so it never reaches a public include.

- **State is internal.** Every subsystem `*State` (`InputState`, `PhysicsState`, `AssetsState`, …) and the
  composed `RuntimeState` live in `tbx::internal`. Users never name a state type; they call free functions.
- **State-consuming functions are internal.** Any function that takes a state by reference — the per-frame
  verbs (`update_*`, `purge_*`) *and* the testable query/command implementations (`is_down(InputState&, …)`,
  `raycast(PhysicsState&, …)`, `load_asset(AssetsState&, …)`) — lives in `tbx::internal`. Taking explicit
  state is what keeps them unit-testable; hiding them keeps the public API clean.
- **The public API is free functions over `current()`.** For each internal state-taker a user needs, expose
  a thin `tbx::` free function that reads `internal::current()` and forwards
  (`bool is_key_down(Key)` → `internal::is_down(internal::current().input, key)`). These are the *only* thing
  users call; they never touch a state.
- **Query escape hatches.** When something a user needs lives only in a state (e.g. `WindowsState::open_windows`),
  expose a public free function that returns the plain data — `tbx::get_open_windows()`, not the state.
- **Reinforces API Leakage.** Per the API-Leakage rule, `internal::` names never appear in a public signature,
  return type, or doc. `internal::current()` returning `internal::RuntimeState&` is itself internal for that reason.
- **Tests may reach in.** Unit tests (and `app.cpp`, the orchestrator) freely name `internal::` types and call
  `internal::` functions — that is the intended seam for driving state directly.
- **Layout.** In a public header the `internal` block (usually just forward-referenced or empty) sits at the
  bottom; in a source file internal constants/structs/helpers sit at the top. Internal declarations live in a
  `namespace tbx::internal { … }` block, ideally in a `src/<module>/<module>_internal.h` companion header.

## Blessed Exceptions

Deliberate, narrow deviations from the rules above — each is load-bearing; do not copy the pattern anywhere new without a matching reason:

- **Backend-seam pImpl**: subsystem state structs (`audio.h`, `physics.h`, `window.h`, `ui.h`) may forward-declare one nested `struct Backend;`/`State;`/`Simulation;` held by `std::unique_ptr` — the ONLY sanctioned way to keep backend types out of public headers. Exempt from the no-forward-declarations and no-nested-types rules. The OS handles for windows hang off `WindowsState`'s backend, so `Window` itself stays pure data.
- **Runtime pImpl**: `runtime.h` forward-declares `namespace tbx::internal { struct RuntimeState; }` and `Runtime` holds `std::unique_ptr<internal::RuntimeState>`, so the public header pulls in no module state. `Runtime`'s destructor and any state accessor are defined out-of-line (in `app.cpp`, where the `_internal.h` headers are visible). This is the one sanctioned cross-namespace forward declaration; it exists to keep `RuntimeState` and every `*State` out of public includes.
- **Cycle-breaking forward declarations**: `class Sandbox;` in `toy.h`/`kit.h` breaks a true circular pair; allowed only where two headers genuinely need each other.
- **PCH**: `src/pch.h` may include third-party seam headers (including `<entt/entt.hpp>`) for build speed; all *usage* still goes through the seam headers.
- **SteamAudio's SDL usage**: the steamaudio backend opens its output device through SDL directly (5 calls + 1 callback). A dedicated platform audio-output seam is the documented swap path if a non-SDL platform backend ever lands; until then the direct calls are the dead-simple choice.
- **`assets::load` specialization blocks**: a `load<T>` specialization must live in `tbx::assets` (the primary's namespace — C2912 otherwise) but belongs next to its type, so spec-carrying headers/sources end with a second trailing `namespace tbx::assets { ... }` block — the ONLY sanctioned multi-namespace-block file shape.

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
- Line endings: git normalizes to LF in the repository; the working tree is mixed (editors write LF, `.clang-format` is configured for CRLF). Don't hand-convert files or mass-reformat for endings alone — that churn buries real changes.
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
    Consts...
    NestedTypes...

    Constructor
    Destructor

    CopyConstructors...
    AssignmentOperators...
  
    Getters/Setters (sort by keyword: static/inline/etc, then by name)
    Methods (sort by keyword: static/inline/etc, then by name)
    Properties (sort by keyword: static/inline/etc, then by name)

  protected:
    Consts...
    NestedTypes...

    Constructor
    Destructor

    CopyConstructors...
    AssignmentOperators...

    Getters/Setters (sort by keyword: static/inline/etc, then by name)
    Methods (sort by keyword: static/inline/etc, then by name)
    Properties (sort by keyword: static/inline/etc, then by name)

  private:
    Consts...
    NestedTypes...

    Constructor
    Destructor

    CopyConstructors...
    AssignmentOperators...

    Getters/Setters (sort by keyword: static/inline/etc, then by name)
    Methods (sort by keyword: static/inline/etc, then by name)
    Properties (sort by keyword: static/inline/etc, then by name)
};
```

Structs should remain simple and if everything is public they are exempt from the above layout, if a struct has private members then it should follow the same format as a class. Structs should be plain ol' data with little or no behavior

Struct with all public members:
```cpp
struct Name
{
  Usings...
  Consts...
  NestedTypes...

  Constructor
  Destructor

  Getters/Setters...
  Methods...
  Properties...
}
```
