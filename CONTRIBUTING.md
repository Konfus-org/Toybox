### Contributing

### Code of Conduct
Read the code of conduct [here](CODE_OF_CONDUCT.md)

### Coding Guidelines
Follow `CODE_STANDARDS.md`

### Build & Test Prerequisites
- Install CMake 3.28.3 or newer.
- Install Ninja.
- Install Clang/LLVM so both `clang` and `clang-format` are available on your `PATH`.

Build and test commands should use the presets in `CMakePresets.json` (for example: `clang`, `msvc`, `clang-debug`, `test-clang-debug`).

See `AGENTS.md` for the AI contributor standards used by AI agents which are allowed and used in this project.
However, AI code is used with GREAT care and scrutiny. All AI-generated code must be reviewed and approved by a human before being merged and any AI-generated code must follow the same standards as human-written code.

### Merging and Approval: 
Make a fork with your desired changes then create a pull request to merge it back into the main repo.
A Toybox team member, will review, offer comments, and approve if all comments are addressed.
