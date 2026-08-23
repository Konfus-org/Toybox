# CLAUDE.md

Hex Engine is a game engine written in Rust.

## Guidelines

### Follow Rust best practices
- Write idiomatic Rust: use the type system, ownership, and `Result`/`Option` instead of fighting them.
- Keep code warning-free under `cargo clippy` and formatted with `cargo fmt`.
- Prefer safe Rust; any `unsafe` block needs a comment justifying why it is sound.
- Public APIs get doc comments and follow the [Rust API Guidelines](https://rust-lang.github.io/api-guidelines/).

### Prioritize existing solutions
- Before writing something new, look for a well-maintained crate that already solves the problem (e.g. `winit` for windowing, `wgpu` for graphics, `glam` for math).
- Before adding a new module or abstraction, check whether existing code in this repo already covers it.
- Only hand-roll a solution when existing options genuinely don't fit, and note why in the code or PR.

### Keep it simple and readable
- Simple and easy to read beats clever or over-engineered. Optimize for the next reader.
- Don't add abstraction layers, traits, or generics for flexibility nobody has asked for yet.
- Small, focused modules and functions; clear names over comments where possible.
- If a design needs a diagram to explain, look for a simpler design first.

## Common commands

- `cargo build` — build the engine
- `cargo test` — run tests
- `cargo clippy --all-targets` — lint
- `cargo fmt` — format
