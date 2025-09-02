## About

Toybox is a simple, light-weight, plugin based, open source game engine that is currently in development.
There isn't much here yet but, hopefully, soon there will be!

## How to build

Toybox now uses [CMake](https://cmake.org/) to generate project files for different build environments.

### Prerequisites

1. Install [CMake](https://cmake.org/download/).
2. Install [Python](https://www.python.org/downloads/).
3. Install the [Vulkan SDK](https://vulkan.lunarg.com/sdk/home).
4. Enable long file path support on Windows if necessary.

### Building

```
mkdir build
cd build
cmake ..
cmake --build .
ctest
```

The compiled binaries can be found in `build/bin`. Open the generated project with your preferred IDE or run the executables directly.

## What's here so far?

Honestly, not much yet. There is a simple ECS system dubbed TBS (Toy Box System), a plugin system that will find and load plugins at runtime, a math library (which is a thin wrapper around GLM), logging and asserts, a windowing system made using SDL, some graphics primitives, and a simple renderer and input system. I plan to add all the things a game engine needs! Physics, audio, networking, a UI system, editor and more!

## Contributing

Make a fork with your desired changes then create a pull request to merge it back into the main repo.
I will review, offer comments, and approve if all comments are addressed.
