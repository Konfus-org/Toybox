# Agent Instructions for Toybox

Welcome to the Toybox game engine repository! This document provides high-level guidance for agents contributing to the project.

## Project Structure Overview
- `Engine/`: Core engine source, including ECS, math, logging, windowing, graphics, and systems code.
- `Launcher/`: Entry points and launcher utilities for running the engine.
- `Examples/`: Sample projects demonstrating engine functionality.
- `Plugins/`: Runtime-discoverable plugins that extend engine capabilities.
- `Dependencies/`: Third-party libraries and external dependencies used by the engine.
- `CMakeLists.txt` and `CMakePresets.json`: Build configuration for generating project files across platforms.
- `Build/`: Generated artifacts (binaries, libraries, pdbs) created during the build process.
- `README.md`: Additional background and contributor information.

## Code Standards
- Follow modern C++20 practices; avoid compiler extensions (`CMAKE_CXX_EXTENSIONS` is `OFF`).
- Follow existing styling and conventions
- Case statements should be indented by 4 spaces (indentation in general should always be 4 spaces) and statements in them should have braces with the break within those braces.
- Avoid C++ attributes unless they are required for platform integration or third-party interoperability.
- Avoid friend classes where possible, if its unavoidable or makes for a better api then its fine to use them but try to avoid if possible.
- Don't nest structs, classes, usings, or enums. Put them above their required class.
- Don't use the explicit keyword, it make the code overly verbose and hard to read.
- Prioritize readability and maintainability.
- Don't use excessive nesting and super long methods and classes, break things up and add comments to explain things.
- Use descriptive names and consistent casing: PascalCase for types, camelCase for functions and variables, and SCREAMING_SNAKE_CASE for constants/macros. Avoid catch-all names such as `Util` and strive to apply SOLID design principles where practical.
- No empty namespaces or detail namespaces, if you want something not exposed put it into a cpp file and make it static.
- Don't use the 'Tbx::' on structs and classes and methods and such when within the namespace, no empty namespaces, no details namespaces, if you want something not exposed put it into a cpp file and make it static.
- Try to keep things easy to understand and reason about, don't use magic numbers and comment any assumptions.
- Defensively program around plugins, they can be unloaded and reloaded at any time so keep that in mind, use TBX_ASSERT to assert when things go wrong, but allow the app to continue and try to recover.
- Prefer ++ after something instead of before to increment eg int++ instead of ++int.
- Keep header/source separation clear; place public interfaces in headers and implementation details in source files.
- Prefer `#pragma once` in headers and minimize unnecessary includes to keep compile times short.
- Ensure newly added code is covered by unit or integration tests when possible, and keep code warnings free.
- Run formatting or linting tools provided in the repository when available. If none exist, maintain the existing code style in surrounding files.
- Verify changes on all major platforms we target (Windows, macOS, and Linux) or provide clear reasoning when platform parity is temporarily unavailable.
- Use windows style line endings

## Repository Workflow
1. **Clone or pull the latest changes**
   ```bash
   git clone https://github.com/Konfus-org/Toybox.git
   git submodule update --init --recursive
   ```
2. **Configure the build** (choose one of the following):
   - **Windows (Visual Studio preset)**
     ```bash
     cmake --preset tbx-vs
     cmake --build --preset tbx-vs
     ```
   - **Other platforms (generic out-of-source build)**
     ```bash
     cmake -S . -B build
     cmake --build build
     ```
3. **Run tests**
   ```bash
   # Using preset
   ctest --preset tbx-vs

   # Generic build directory
   ctest --test-dir build
   ```
4. **Prepare your pull request**
   - Ensure the working tree is clean (`git status`).
   - Confirm all required tests and linters have passed.
