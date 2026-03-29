# Toybox CodeStandard

## Language
- Target C++23.
- Do not use C++ attributes (for example `[[nodiscard]]`).
- Do not use `explicit` on constructors.
- Prefer copy-style initialization.
- Do not use blanket namespace imports.

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

## Class / Struct Layout
Use this ordering for every class/struct:

```cpp
class/struct Name :
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

## Type Organization
- Do not nest classes or structs inside other classes/structs.
- Move nested helper types to top-level declarations within the same namespace.

## Formatting
- Follow root `.clang-format`.
- Use LF line endings.
- Keep `#include` directives contiguous.
- Prefer simple, flat control flow and remove unnecessary nesting.
