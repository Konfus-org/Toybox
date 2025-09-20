## About

Toybox is a simple, light-weight, plugin based, open source game engine that is currently in development.
There isn't much here yet but, hopefully, soon there will be!

## Repository Structure

- `Engine/`: Core engine source, including ECS, math, logging, windowing, graphics, and systems code.
- `Launcher/`: Entry points and launcher utilities for running the engine.
- `Examples/`: Sample projects demonstrating engine functionality.
- `Plugins/`: Runtime-discoverable plugins that extend the engine at runtime.
- `Dependencies/`: Third-party libraries consumed by Toybox.
- `CMakeLists.txt` and `CMakePresets.json`: Top-level build configuration.

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

#### Windows

The repository provides a CMake preset that generates Visual Studio files in the `Generated` folder:

```bash
# Generate VS project in "Generated" folder
cmake --preset tbx-vs

# Compile Toybox
cmake --build --preset tbx-vs

# Run tests
ctest --preset tbx-vs
```

#### Other platforms

```bash
# Build to 'build' directory
cmake -S . -B build

# Compile Toybox
cmake --build build

# Run tests
ctest --test-dir build
```

Compiled binaries are placed in `Build/bin`, and libraries in `Build/lib`. Open the generated project with your preferred IDE or run the executables directly. Please ensure changes build cleanly and tests pass on the platforms you touch (Windows, macOS, and Linux).

## Coding Guidelines

- Target modern C++20 without compiler extensions or C++ attributes unless required for platform compatibility.
- Keep interfaces in headers and implementation details in source files, preferring `#pragma once`.
- Use descriptive names (avoid catch-all names like `Util`) and follow SOLID principles where practical.
- Maintain existing formatting conventions and keep builds free of warnings.
- Add automated tests when introducing new functionality.

## What's here so far?

Honestly, not much yet. There is a simple ECS system dubbed TBS (Toy Box System), a plugin system that will find and load plugins at runtime, a math library (which is a thin wrapper around GLM), logging and asserts, a windowing system made using SDL, some graphics primitives, and a simple renderer and input system. I plan to add all the things a game engine needs! Physics, audio, networking, a UI system, editor and more!

## Contributing

Make a fork with your desired changes then create a pull request to merge it back into the main repo.
I will review, offer comments, and approve if all comments are addressed.
