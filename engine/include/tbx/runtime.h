#pragma once
#include "tbx/api.h"
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
#include "tbx/scripting/scripts.h"
#include "tbx/ui/ui.h"
#include <chrono>
#include <memory>

namespace tbx
{
    /// @brief
    /// Purpose: Move-only value handle over stable state — moves are pointer swaps, so the
    /// functional dialect (state-advancing verbs consume and return the runtime) costs
    /// nothing and never invalidates async captures of the state. The state itself is an
    /// internal pImpl (internal::RuntimeState); external code never names it — it uses the
    /// free-function API (tbx::spawn, tbx::is_key_down, tbx::run, …). One Runtime with a
    /// window or UI at a time: the platform and UI libraries underneath are process-global.
    struct TBX_DLL_EXPORT Runtime
    {
        Runtime();
        explicit Runtime(App app);

        std::unique_ptr<internal::RuntimeState> state;
    };

    /// @brief
    /// Purpose: Runs one frame: presents the previous one, pumps OS events/jobs/events,
    /// updates streaming/scripts/fixed-step, and renders through the app's graph (unless
    /// graphics.is_custom_pipeline — then the host draws). The first call boots
    /// (CREATED -> RUNNING); false means the app stopped — teardown is the Runtime's
    /// destructor. THE loop: `while (tbx::run(runtime)) { ... }`.
    TBX_DLL_EXPORT bool run(Runtime& runtime);

    /// @brief
    /// Purpose: Requests a clean exit — the next run() stops the app.
    TBX_DLL_EXPORT void quit(Runtime& runtime);

    /// @brief
    /// Purpose: Requests the current running runtime to close and purge — the next run() stops it.
    /// The parameterless exit scripts and game code call (tbx.quit()); no handle needed.
    TBX_DLL_EXPORT void quit();

    // ---- Internal (engine machinery; not the user-facing API) ----
    namespace internal
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
        /// Purpose: Everything the engine owns, as plain state behind the Runtime pImpl — the
        /// engine and tests read/write it directly (runtime.physics.gravity); external code goes
        /// through the free-function API instead. Declaration order IS the dependency graph and
        /// reverse-order destruction IS shutdown: the module states (declared last) die first — ui
        /// (Lua-closure bindings) before scripts, scripts (the VMs) before the sandbox, renderer
        /// (GPU caches) before the windows and their shared GL context, the assets watcher before
        /// the jobs pool it posts through, and jobs last of the modules. Do not reorder members
        /// without re-deriving that sequence. Lives at a stable heap address behind Runtime so
        /// async work (watcher, workers, coroutines, Lua) survives Runtime moves.
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

        /// @brief
        /// Purpose: The one running runtime, published by run() for the frame. The deliberate single
        /// global that lets the script-facing engine API be plain free functions instead of threading
        /// state. MAIN THREAD ONLY — async work must capture the state it was handed, never call this.
        TBX_DLL_EXPORT RuntimeState& current();

        /// @brief
        /// Purpose: True while a runtime is published (between the first run() and shutdown) — guard
        /// for convenience helpers that may run before boot or after teardown.
        TBX_DLL_EXPORT bool has_current();
    }
}
