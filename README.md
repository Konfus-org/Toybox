## About

Toybox is a simple, light-weight, plugin based, open source game engine that is currently in development.
There isn't much here yet but, hopefully, soon there will be!

## How to build

Toybox now uses [CMake](https://cmake.org/) to generate project files for different build environments.

### Prerequisites

1. Install [CMake](https://cmake.org/download/).
2. Install [Python](https://www.python.org/downloads/).
3. Ensure your system supports OpenGL (nearly everything does, so you should be good).
4. Enable long file path support on Windows if necessary.

### Building

#### Windows

The repository provides a CMake preset that generates Visual Studio files in the `Generated` folder:

```bash
cmake --preset tbx
cmake --build --preset tbx
ctest --preset tbx
```

#### Other platforms

```bash
# Configure the project and generate build files in the `build` directory
cmake -S . -B build

# Compile the engine using the default build type
cmake --build build

# Run the test suite
ctest --test-dir build
```

Compiled binaries are placed in `Build/bin`, and libraries in `Build/lib`. Open the generated project with your preferred IDE or run the executables directly.

## What's here so far?

Honestly, not much yet. There is a simple ECS system dubbed TBS (Toy Box System), a plugin system that will find and load plugins at runtime, a math library (which is a thin wrapper around GLM), logging and asserts, a windowing system made using SDL, some graphics primitives, and a simple renderer and input system. I plan to add all the things a game engine needs! Physics, audio, networking, a UI system, editor and more!

## Contributing

Make a fork with your desired changes then create a pull request to merge it back into the main repo.
I will review, offer comments, and approve if all comments are addressed.
