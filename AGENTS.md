# Toybox Agent Guide

Operational guide for AI agents working in this repository. AI-generated code is held to the **same standards as human-written code** and is reviewed with great care — see [`docs/Contributing.md`](docs/Contributing.md).

## Standards

**Strictly follow [`docs/CodeStandards.md`](docs/CodeStandards.md).** It is the single source of truth for engineering policies, C++ rules, unit-test style, file/class layout, and formatting. Do not restate those rules here — keep `docs/` up to date when making sweeping architectural changes.

Most-violated reminders (full rules live in CodeStandards):

- C++23 only; no blanket `using namespace` imports.
- No `detail`/`internal` namespaces and no anonymous namespaces — use file-scope `static` instead.
- Use `size` and `uint` from `tbx/types/typedefs.h`, never raw `std::size_t`.
- Permanently delete stale code; never leave commented-out placeholders.
- Prefer existing engine utilities and the simplest direct solution over new abstractions.

## Repository Map

- `engine/` — first-class logic compiled directly into the engine.
- `plugins/` — runtime-loadable plugins (SDL windowing/input, asset/model/image loaders, OpenGL rendering, physics, profiling).
- Sample content lives outside this repo: `../ExampleProject` (museum app) follows the standard project layout (CMakeLists, AppSettings.json, Assets/, Source/) and builds against this engine via `-DTBX_ENGINE_DIR`.
- `resources/` — shared engine resources and generated resource code (shaders live here).
- `launcher/` — launcher executable that hosts apps.
- `cmake/`, `tools/` — shared CMake/codegen utilities and `run_and_capture.ps1`.
- `thirdparty/` — vendored dependencies.
- `CMakePresets.json` — all configure/build/test presets. Build exclusively through these presets; no ad-hoc `cmake` invocations.

## Build / Test / Run

Normal debug presets are intentionally unsanitized for framerate. Use the sanitized presets for memory-safety / UB / startup validation.

**Sanitized build + tests (ASan+UBSan):**

```
cmake --preset clang-sanitize-tests
cmake --build --preset clang-sanitize-debug-tests
ctest --preset test-clang-sanitize-debug
```

**Normal debug tests:**

```
cmake --preset clang-tests
cmake --build --preset clang-debug-tests
ctest --preset test-clang-debug
```

**Build & run the example project (lives at `../ExampleProject`):**

```
cmake -S ../ExampleProject -B ../ExampleProject/build -G "Ninja Multi-Config" -DTBX_ENGINE_DIR=<engine dir> -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build ../ExampleProject/build --config Debug --parallel
```

Then launch `../ExampleProject/build/bin/Debug/Launcher.exe --app=ExampleProject --settings=<abs path to ExampleProject/AppSettings.json>`.

## Verification

- **Always test changes**: build, run `ctest` (presets above), then launch the example app, let it run a few seconds, and shut it down. Examine the build dir's `logs/` folder — fix any logged warnings or errors and re-test until the logs are clean.
- **Visual verification (rendering changes)**: run `tools/run_and_capture.ps1`. It writes `run_screenshot.png`, `run_stdout.log`, and `run_stderr.log` to `build/run_and_capture`. Always visually validate rendering changes.
- **Profiling**: when asked to profile or debug performance, run `VSDiagnostics.exe` against the example app. Capture actionable CPU/GPU/frame-time evidence before proposing fixes, then re-verify the same scenario after the change.
