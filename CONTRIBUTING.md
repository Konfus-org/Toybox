### Contributing

### Code of Conduct
Read the code of conduct [here](CODE_OF_CONDUCT.md)

### Coding Guidelines

- Format with the root `.clang-format` (Allman braces, 4-space indents, namespace indentation, sorted includes) and keep files using Windows-style (CRLF) endings.
- Keep namespaces meaningful—avoid empty namespaces and prefer `static` helpers for TU-local utilities.
- Use descriptive, self-documenting names (favor `Get...`/`Set...` for accessors) rather than jargon or `Util`-style blobs.
- Document every public API with purpose, ownership/lifetime expectations, and thread-safety notes.
- Prefer Toybox smart handles (`Ref`, `WeakRef`, `Scope`) over raw owning pointers; raw pointers should remain non-owning and clearly documented.
- We build with C++23 but stick to a conservative, C-like subset. Avoid `noexcept` and attribute annotations, and only adopt modern features when they clearly improve safety or clarity.
- Keep headers lean by defining non-trivial logic in `.cpp` files and add tests when introducing new functionality.

See `AGENT.md` for the complete contributor standards used by Codex agents.

### Merging and Approval: 
Make a fork with your desired changes then create a pull request to merge it back into the main repo.
A Toybox team member, will review, offer comments, and approve if all comments are addressed.
