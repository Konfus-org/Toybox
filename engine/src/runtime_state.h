#pragma once
#include "tbx/app.h"
#include "tbx/audio/audio.h"
#include "tbx/debug/debugging.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/events/events.h"
#include "tbx/gfx/state.h"
#include "tbx/jobs/jobs.h"
#include "tbx/physics/physics.h"
#include "tbx/platform/input.h"
#include "tbx/platform/window.h"
#include "tbx/runtime.h"
#include "tbx/scripting/scripts.h"
#include "tbx/ui/ui.h"
#include <chrono>

// The engine's whole state, kept OUT of the public runtime.h so external code never sees it — the
// Runtime handle forward-declares internal::RuntimeState and holds it by unique_ptr (pImpl). The
// engine's own sources and the tests include THIS header to touch the state directly; everyone else
// uses the free-function API (tbx::spawn, tbx::is_key_down, tbx::run, …).
namespace tbx::internal
{
    /// @brief
    /// Purpose: Loop pacing — the frame clock and the fixed-step accumulator.
    struct TBX_DLL_EXPORT FrameState
    {
        uint64 index = 0;
        std::chrono::steady_clock::time_point previous = {};
        float delta_time = 0.0f;
        float accumulator = 0.0f;
    };

    /// @brief
    /// Purpose: Everything the engine owns, as plain state behind the Runtime pImpl — the engine and
    /// tests read/write it directly (runtime.physics.gravity); external code goes through the
    /// free-function API instead. Declaration order IS the dependency graph and reverse-order
    /// destruction IS shutdown: the module states (declared last) die first — ui (Lua-closure
    /// bindings) before scripts, scripts (the VMs) before the sandbox, renderer (GPU caches) before
    /// the windows and their shared GL context, the assets watcher before the jobs pool it posts
    /// through, and jobs last of the modules. Do not reorder members without re-deriving that
    /// sequence. Lives at a stable heap address behind Runtime so async work (watcher, workers,
    /// coroutines, Lua) survives Runtime moves.
    struct TBX_DLL_EXPORT RuntimeState
    {
        App app = {};
        Sandbox sandbox = {};
        FrameState frame = {};
        InputState input = {};
        WindowsState windows = {};
        JobsState jobs = {};
        EventsState events = {};
        AssetsState assets = {};
        RenderState renderer = {};
        ScriptsState scripts = {};
        PhysicsState physics = {};
        AudioState audio = {};
        UiState ui = {};
        DebuggingState debug = {};
    };
}
