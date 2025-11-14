## About

Toybox is a simple, light-weight, plugin based, open source game engine that is currently in development.
There isn't much here yet but, hopefully, soon there will be!

## What's here so far?

Honestly, not much yet. There is a simple ECS system dubbed TBS (Toy Box System), a plugin system that will find and load plugins at runtime, a math library (which is a thin wrapper around GLM), logging and asserts, a windowing system made using SDL, some graphics primitives, and a simple renderer and input system. I plan to add all the things a game engine needs! Physics, audio, networking, a UI system, editor and more!

## Repository Structure

- `modules/`: First-class engine modules. `app/` hosts the runtime loop and timers, `messaging/` powers the dispatcher, `debugging/` implements logging and trace helpers, `os/` wraps platform concepts such as windows and shared libraries, `plugin_api/` exposes the plugin contract, and `std/` contains the Toybox standard utilities.
- `examples/`: Sample projects demonstrating engine functionality.
- `plugins/`: Runtime-discoverable plugins that extend the engine.
- `thirdparty/`: Vendored dependencies.
- `cmake/`, `CMakeLists.txt`, `CMakePresets.json`: Build configuration and presets.

## Modules vs Plugins

Toybox now distinguishes between *modules* and *plugins*.

- **Modules** ship as part of the engine and live under `modules/`. They define the core behaviour (messaging, windowing, debugging, etc.) that every Toybox application relies on. Replacing a module requires rebuilding the engine because the modules are compiled directly into the host binaries.
- **Plugins** reside in `plugins/` and follow the dynamic plugin contract. They are designed to be swapped, extended, or omitted without recompiling the engine. At runtime the application can selectively load plugins that implement optional features such as SDL integration or logging backends.

## Toybox Standard Library (TSL)

Toybox ships with a small standard library module that provides consistent building blocks across the engine and plugins:

- `tbx/std/string.h` exposes `tbx::String` plus helpers like `get_trimmed` and `get_lower_case`.
- `tbx/std/list.h` and `tbx/std/array.h` add lightweight container wrappers with familiar C-style semantics and a `std_vec()`/`std_array()` escape hatch when raw access is needed.
- `tbx/std/smart_pointers.h` defines `tbx::Scope`, `tbx::Ref`, and `tbx::WeakRef` for deterministic ownership, aligned with the engine coding guidelines.
- `tbx/std/casting.h` extends the casting helpers (`is`, `as`, `try_as`) to work with raw pointers, smart pointers, and `tbx::Any`.

These headers live under `modules/std/include/tbx/std` and are reused throughout the engine/tests/plugins. Prefer them over the raw STL types when adding new code so behaviour stays predictable across platforms.

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

Artifacts land in `cmake/build/bin/<Config>` with matching `lib` and `pdb` directories.

## Contributing

Look to the contributing documentation [here](CONTRIBUTING.md)
