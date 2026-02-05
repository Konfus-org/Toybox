## About

Toybox is a simple, light-weight, plugin based, open source game engine that is currently in development.

There isn't much here yet but, hopefully, soon there will be!

## What's here so far?

Honestly, not much yet. There is a simple ECS system dubbed TBS (Toy Box System), a plugin system that will find and load plugins at runtime, a math library (which is a thin wrapper around GLM), logging and asserts, a windowing system made using SDL, some graphics primitives, and a simple renderer and input system. I plan to add all the things a game engine needs! Physics, audio, networking, a UI system, editor and more!

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
3. Install [Python](https://www.python.org/downloads/).
4. Ensure your system supports OpenGL (nearly everything does, so you should be good).
5. Enable long file path support on Windows if necessary.

### Building

### CMake version notes

CMake 4.1.0 or newer is required to support the Ninja Multi-Config presets.

All Toybox presets use Ninja for faster builds. The Clang preset requires
Clang/LLVM to be installed and available on your PATH. Install it here:
- [Clang/LLVM](https://llvm.org/)

Toybox relies on the presets defined in `CMakePresets.json`. Configure once, then build/test via the associated build and test presets.

#### Cross-platform Ninja/Clang (Recommended)

```bash
# Configure (builds under build/tbx-clang)
cmake --preset tbx-clang

# Build Debug or Release
cmake --build --preset tbx-clang-debug
cmake --build --preset tbx-clang-release

# Run tests
ctest --preset tbx-test-clang-debug
ctest --preset tbx-test-clang-release
```

Artifacts land in `build/tbx-clang/bin/<Config>` with matching `lib` and `pdb` directories.

#### MSVC (Ninja + MSVC toolchain)

```bash
# Configure (builds under build/tbx-msvc)
cmake --preset tbx-msvc

# Build Debug or Release
cmake --build --preset tbx-msvc-debug
cmake --build --preset tbx-msvc-release

# Run tests
ctest --preset tbx-test-msvc-debug
ctest --preset tbx-test-msvc-release
```

#### Default (Auto toolchain)

```bash
# Configure (builds under build/tbx-default)
cmake --preset tbx-default

# Build Debug or Release
cmake --build --preset tbx-default-debug
cmake --build --preset tbx-default-release

# Run tests
ctest --preset tbx-test-default-debug
ctest --preset tbx-test-default-release
```

## Contributing and AI Usage

Look to the contributing documentation [here](CONTRIBUTING.md).

In regards to AI usage:

See `AGENT.md` for the AI contributor standards used by AI agents which are allowed and used in this project. We recongnize the potential of AI tools to assist in development and encourage their responsible use.
However, AI code is used with great care and scrutiny and is never blindly accepted, if you use AI to contribute you should fully understand what the code it gives you is doing and be ready to explain, defend, and/or change it.
All AI-generated code must be reviewed and approved by a human before being merged and any AI-generated code must follow the same standards as human-written code.