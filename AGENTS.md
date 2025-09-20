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
- Avoid C++ attributes unless they are required for platform integration or third-party interoperability.
- Keep header/source separation clear; place public interfaces in headers and implementation details in source files.
- Prefer `#pragma once` in headers and minimize unnecessary includes to keep compile times short.
- Use descriptive names and consistent casing: PascalCase for types, camelCase for functions and variables, and SCREAMING_SNAKE_CASE for constants/macros. Avoid catch-all names such as `Util` and strive to apply SOLID design principles where practical.
- Ensure newly added code is covered by unit or integration tests when possible, and keep code warnings free.
- Run formatting or linting tools provided in the repository when available. If none exist, maintain the existing code style in surrounding files.
- Verify changes on all major platforms we target (Windows, macOS, and Linux) or provide clear reasoning when platform parity is temporarily unavailable.

## Repository Workflow
1. **Clone or pull the latest changes**
   ```bash
   git clone https://github.com/Konfus-org/Toybox.git
   # or, from an existing clone
   git pull origin work
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
