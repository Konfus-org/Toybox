## About

Toybox is a simple, light-weight, plugin based, open source game engine that is currently in development.

There isn't much here yet but, hopefully, soon there will be!

## What's here so far?

Honestly, not much yet. There is a plugin system that will find and load plugins at runtime, an ECS system via EnTT, math via GLM, logging and asserts via spdlog, windowing and input systems made using SDL, graphics primitives, physics via Jolt, and a simple renderer using OpenGL. I plan to add all the things a game engine needs: audio, networking, a UI system, editor tooling, and more.

## Repository Structure

- `examples/`: Sample projects demonstrating engine functionality.
- `modules/`: First-class logic that is directly linked to and compiled into the engine.
- `plugins/`: Runtime-discoverable plugins that extend the engine.
- `thirdparty/`: Vendored dependencies.
- `cmake/`, `CMakeLists.txt`, `CMakePresets.json`: Build configuration and presets.

## Modules vs Plugins

Toybox has two central concepts, *modules* and *plugins*.

- **Modules** ship as part of the engine and live under `modules/`. They define the core behaviour (messaging, windowing, debugging, etc.) that every Toybox application relies on. Replacing a module requires rebuilding the engine as well as plugins that utilize the modules because the modules are compiled directly into the binaries.
- **Plugins** reside in `plugins/` and follow the dynamic plugin contract. They are designed to be swapped, extended, or omitted without recompiling the engine. At runtime the application can selectively load plugins that implement optional features such as SDL integration or logging backends.


## Getting Started

Clone the repository and its submodules:

```bash

git clone https://github.com/Konfus-org/Toybox.git

cd Toybox

git submodule update --init --recursive

```

If you are updating an existing clone, run `git pull origin BRANCH_HERE` followed by `git submodule update --init --recursive`.

## How to build

### Prerequisites

1. Install [CMake](https://cmake.org/download/).
2. Install [Ninja](https://ninja-build.org/).
3. Install [Clang/LLVM](https://llvm.org/) (includes `clang` and `clang-format`, which is required for consistent formatting across the TBX codebase).
4. Install [Python](https://www.python.org/downloads/).
5. Ensure your system supports OpenGL (nearly everything does, so you should be good).
6. Enable long file path support on Windows if necessary.

### Building

### CMake version notes

CMake 3.28.3 or newer is required (matching `CMakePresets.json`).

All Toybox presets use Ninja Multi-Config for faster builds.

Toybox relies on the presets defined in `CMakePresets.json`. Configure once, then build/test via the associated build and test presets.

To inspect available presets on your machine, run:

```bash
cmake --list-presets
```

#### Cross-platform Ninja/Clang (Recommended)

```bash
# Configure (builds under build/clang)
cmake --preset clang

# Build Debug or Release
cmake --build --preset clang-debug
cmake --build --preset clang-release

# Run tests
ctest --preset test-clang-debug
ctest --preset test-clang-release
```

#### MSVC (Ninja + MSVC toolchain)

```bash
# Configure (builds under build/msvc)
cmake --preset msvc

# Build Debug or Release
cmake --build --preset msvc-debug
cmake --build --preset msvc-release

# Run tests
ctest --preset test-msvc-debug
ctest --preset test-msvc-release
```

## Contributing and AI Usage

Look to the contributing documentation [here](CONTRIBUTING.md).

In regards to AI usage:

See `AGENTS.md` for the AI contributor standards used by AI agents which are allowed and used in this project. We recongnize the potential of AI tools to assist in development and encourage their responsible use.
However, AI code is used with great care and scrutiny and is never blindly accepted, if you use AI to contribute you should fully understand what the code it gives you is doing and be ready to explain, defend, and/or change it.
All AI-generated code must be reviewed and approved by a human before being merged and any AI-generated code must follow the same standards as human-written code.
