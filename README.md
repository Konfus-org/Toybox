## About
Toybox is a lightweight, plugin-based, open source game engine that is currently in development.

## What's here so far?
Toybox currently includes a runtime plugin system, ECS integration through EnTT, math via GLM, logging and asserts via spdlog, asset and resource handling, SDL-backed windowing and input plugins, OpenGL rendering, shader include transformation, model/image loader plugins, profiling support, and Jolt-based physics.

## Repository Structure
- `engine/`: First-class logic that is directly linked to and compiled into the engine.
- `plugins/`: Runtime-loadable plugins that extend the engine.
- `examples/`: Sample projects demonstrating engine functionality.
- `resources/`: Shared engine resources and generated resource code.
- `cmake/`: Shared CMake utilities, plugin registration, asset bundling, and code generation helpers.
- `thirdparty/`: Vendored dependencies.
- `CMakeLists.txt`, `CMakePresets.json`: Top-level build configuration and supported configure/build/test presets.

## Plugins
Toybox revolves around *plugins*.
Plugins are shared libraries with generated metadata that follow the dynamic plugin contract.
They are designed to be swapped, extended, or omitted without recompiling the engine.
At runtime, applications can selectively load plugins that implement optional features such as SDL integration, asset loading, physics, profiling, or graphics backends.
The engine also supports reloading plugins at runtime.

## Getting Started
Clone the repository and its submodules:

```bash
git clone https://github.com/Konfus-org/Toybox.git
cd Toybox
git submodule update --init --recursive
```

If you are updating an existing clone, pull the branch you are working on and then run:

```bash
git submodule update --init --recursive
```

## How to build

### Prerequisites
1. Install [CMake](https://cmake.org/download/) 3.28.3 or newer.
2. Install [Ninja](https://ninja-build.org/).
3. Install a C++23 compiler supported by the presets:
   - [Clang/LLVM](https://llvm.org/) for the `clang` preset.
   - MSVC Build Tools or Visual Studio with C++ support for the `msvc` preset on Windows.
4. Install [Python](https://www.python.org/downloads/).
5. Ensure your system supports OpenGL.
6. Enable long file path support on Windows if necessary.
7. Optional: install `sccache` or `ccache`; the presets enable compiler cache launchers when available.

### Building

All Toybox presets use Ninja Multi-Config for faster builds.
Configure once, then build and test via the associated presets from `CMakePresets.json`.

To inspect available presets on your machine, run:

```bash
cmake --list-presets
cmake --list-presets=build
cmake --list-presets=test
```

#### Clang
```bash
# Configure; output goes under build/clang
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
# Configure; output goes under build/msvc
cmake --preset msvc

# Build Debug or Release
cmake --build --preset msvc-debug
cmake --build --preset msvc-release

# Run tests
ctest --preset test-msvc-debug
ctest --preset test-msvc-release
```

## Contributing and AI Usage
Look to the contributing documentation [here](docs/Contributing.md).

In regards to AI usage:

See `AGENTS.md` for the AI contributor standards used by AI agents in this project.
We recognize the potential of AI tools to assist in development and encourage their responsible use.
However, AI code is used with great care and scrutiny and is never blindly accepted. If you use AI to contribute, you should fully understand what the code is doing and be ready to explain, defend, and/or change it.
All AI-generated code must be reviewed and approved by a human before being merged and any AI-generated code must follow the same standards as human-written code.
