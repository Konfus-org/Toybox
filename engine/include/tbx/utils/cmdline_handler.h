#pragma once
#include "tbx/api.h"
#include "tbx/app.h"
#include "tbx/runtime.h"

// The built-in command line: hosts hand main()'s arguments to App::commands (a parsed
// CommandList) and the runtime honors these options everywhere:
//   -w N / -h N          window size overrides, applied before the window exists
//   --screenshot[=path]  capture real rendered frames right before they present, then quit
//   -number N            how many screenshots (default 1; files numbered _1.._N when N > 1)
//   -delay F             frames between captures (default 8 — warm-up so first-frame asset
//                        loads land before the first capture)
namespace tbx::cmdline
{
    /// @brief
    /// Purpose: Applies parse-time options onto the app (window size overrides) — the
    /// Runtime constructor calls it before the window is created.
    TBX_API void apply(App& app);

    /// @brief
    /// Purpose: Per-frame command handling (--screenshot captures) — run() calls it at
    /// frame start, while the backbuffer still holds the previous frame's image.
    TBX_API void update(RuntimeState& state);
}
