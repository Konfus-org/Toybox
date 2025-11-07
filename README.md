## About

Toybox is a simple, light-weight, plugin based, open source game engine that is currently in development.
There isn't much here yet but, hopefully, soon there will be!

## What's here so far?

Honestly, not much yet. There is a simple ECS system dubbed TBS (Toy Box System), a plugin system that will find and load plugins at runtime, a math library (which is a thin wrapper around GLM), logging and asserts, a windowing system made using SDL, some graphics primitives, and a simple renderer and input system. I plan to add all the things a game engine needs! Physics, audio, networking, a UI system, editor and more!

## Repository Structure

- `engine/`: Core engine source (ECS, math, logging, windowing, graphics, systems).
- `examples/`: Sample projects demonstrating engine functionality.
- `plugins/`: Runtime-discoverable plugins that extend the engine.
- `thirdparty/`: Vendored dependencies.
- `cmake/`, `CMakeLists.txt`, `CMakePresets.json`: Build configuration and presets.

## Getting Started

Toybox uses [CMake](https://cmake.org/) to generate project files for different build environments.

```bash
git clone https://github.com/<your-org>/Toybox.git
cd Toybox

# Ensure all submodules are present
git submodule update --init --recursive
```

If you are updating an existing clone, run `git pull origin work` followed by `git submodule update --init --recursive`.

## How to build

### Prerequisites

1. Install [CMake](https://cmake.org/download/).
2. Install [Python](https://www.python.org/downloads/).
3. Ensure your system supports OpenGL (nearly everything does, so you should be good).
4. Enable long file path support on Windows if necessary.

### Building

Toybox relies on the presets defined in `CMakePresets.json`. Configure once, then build/test via the associated build and test presets.

#### Windows (Visual Studio 2022)

```bash
# Configure (creates files under cmake/build/vs)
cmake --preset tbx-vs

# Build Debug or Release
cmake --build --preset tbx-vs-debug
cmake --build --preset tbx-vs-release

# Run tests
ctest --preset tbx-test-vs-debug
ctest --preset tbx-test-vs-release
```

#### Cross-platform Ninja workflow

```bash
# Configure (under cmake/build/ninja)
cmake --preset tbx-ninja

# Build Debug or Release
cmake --build --preset tbx-ninja-debug
cmake --build --preset tbx-ninja-release

# Run tests
ctest --preset tbx-test-ninja-debug
ctest --preset tbx-test-ninja-release
```

Artifacts land in `cmake/build/bin/<Config>` with matching `lib` and `pdb` directories. Ensure the configuration you touched builds and passes tests before sending changes.

## Contributing

Look to the contributing documentation [here](CONTRIBUTING.md)
