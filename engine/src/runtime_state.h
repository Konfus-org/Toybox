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
// The relocated internal machinery (src/<module>/<module>_internal.h): aggregated here so any TU
// with the full runtime (app.cpp, tests) sees the per-frame verbs without pulling each module's
// internal header itself.
#include "assets/assets_internal.h"
#include "audio/audio_internal.h"
#include "debug/debugging_internal.h"
#include "ecs/ecs_internal.h"
#include "events/events_internal.h"
#include "gfx/gpu_internal.h"
#include "jobs/jobs_internal.h"
#include "physics/physics_internal.h"
#include "platform/input_internal.h"
#include "platform/window_internal.h"
#include "reflection/reflection_internal.h"
#include "scripting/scripts_internal.h"
#include "serialization/serializers_internal.h"
#include "ui/ui_internal.h"
#include "utils/cmdline_internal.h"
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
        // The .tapp this runtime boots from, when constructed via Runtime(AssetHandle<App>): boot()
        // derives the asset root from its folder and load_now's the app config. Empty when the
        // App was supplied directly (Runtime(App)).
        AssetHandle<App> app_source = {};
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
