# Toybox CodeStandard

## General
- Target C++23.
- Do not use C++ attributes (for example `[[nodiscard]]`).
- Do not use `explicit` on constructors.
- Prefer () style init for structs and classes over {} ALWAYS. Only use {} when doing simple inits like auto my_var = {}; or when using .prop_name = prop_val style init to improve readability
- Do not use blanket namespace imports.
- Do not use the std::size_t or uints, instead prefer the simpler using signatures in common/typedefs.h such as `size` or `uint`
- Update anything not meeting these standards if you run into it.

## Documentation
- Use Doxygen `///` summaries only for:
  - `struct` declarations.
  - `class` declarations.
  - Public methods.
- Simple properties may use `//` comments when helpful.
- Do not add documentation comments to private members.
- Remove unnecessary comments and summaries.
- Keep Doxygen summaries directly adjacent to their declaration (no blank line between summary and declaration).
- Plugin and example lifecycle methods (`attach`, `detach`, `update`, including `on_attach`, `on_detach`, `on_update`, `on_fixed_update`) do not require Doxygen summaries.

## File Layout:

``` cpp
#pragma once // do not use old style ifdefs
#includes... // <> for external, "" for internal, should be sorted by name. If order matters then wrap in // clang-format off ... // clang-format on comments

// Internal rules:
// Never expose internal in your return types or public documentation.
// Never allow external consumer code to depend on a internal namespace.
// Do not put headers inside an internal namespace; always restrict those to the .cpp source files
// Should be within its own /internal folder and _internal version of the source files.
namespace tbx::internal
{
    Usings...
    Methods (should always be static in internal namespace)
    Structs...
    Classes...
}

// the public API
namespace tbx
{
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
- Do not nest classes or structs inside other classes/structs.
- Move nested helper types to top-level declarations within the same namespace.

## Formatting
- Follow root `.clang-format`.
- Use LF line endings.
- Keep `#include` directives contiguous.
- Prefer simple, flat control flow and remove unnecessary nesting.

## Graphics Pipeline
Refer to Shader Pipeline Docs [here](ShaderPipeline.md)
