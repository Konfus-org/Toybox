#pragma once
#include "tbx/app.h"
#include "tbx/audio/audio.h"
#include "tbx/debug/view.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/events/events.h"
#include "tbx/gpu/render_graph.h"
#include "tbx/gpu/state.h"
#include "tbx/jobs/jobs.h"
#include "tbx/physics/physics.h"
#include "tbx/platform/input.h"
#include "tbx/platform/window.h"
#include "tbx/scripting/scripts.h"
#include "tbx/ui/ui.h"
#include "tbx/api.h"
#include <chrono>
#include <memory>

namespace tbx
{
    /// @brief
    /// Purpose: Loop pacing — the frame clock and the fixed-step accumulator, queried as
    /// frame.previous / frame.accumulator.
    struct TBX_API FrameState
    {
        uint64 index = 0;
        std::chrono::steady_clock::time_point previous = {};
        float delta_time = 0.0f;
        float accumulator = 0.0f;
    };

    /// @brief
    /// Purpose: Everything the engine owns, as plain public state — read and write it
    /// directly (runtime.events.key.emit(...), runtime.physics.gravity). Declaration order
    /// IS the dependency graph and reverse-order destruction IS shutdown: the module states
    /// (declared last) die first — ui (Lua-closure bindings) before scripts, scripts (the
    /// VMs) before the sandbox, renderer (GPU caches) before the windows and their shared GL
    /// context, the assets watcher before the jobs pool it posts through, and jobs last of
    /// the modules. Do not reorder members without re-deriving that sequence. Lives at a
    /// stable heap address behind Runtime so async work (watcher, workers, coroutines, Lua)
    /// survives Runtime moves.
    struct TBX_API RuntimeState
    {
        App app = {};
        FrameState frame = {};
        input::State input = {};
        windows::State windows = {};
        Sandbox sandbox = {};
        gpu::RenderGraph render_graph = {};
        jobs::State jobs = {};
        events::State events = {};
        assets::State assets = {};
        gpu::State renderer = {};
        scripts::State scripts = {};
        physics::State physics = {};
        audio::State audio = {};
        ui::State ui = {};
        debug::view::State debug = {};
    };

    /// @brief
    /// Purpose: Move-only value handle over stable state — moves are pointer swaps, so the
    /// functional dialect (state-advancing verbs consume and return the runtime) costs
    /// nothing and never invalidates async captures of the state. Module functions take
    /// RuntimeState& and the handle converts implicitly, so call sites pass the runtime
    /// either way; async work (watcher, workers, coroutines, Lua) must capture RuntimeState&,
    /// never this handle — the handle's pointer is in motion between frames. One Runtime with
    /// a window or UI at a time: the platform and UI libraries underneath are process-global.
    struct TBX_API Runtime
    {
        Runtime();
        explicit Runtime(App app);

        /// @brief
        /// Purpose: The handle IS its state to every module function; const on the handle is
        /// the caller-facing dialect (queries and cache fills take the runtime as const).
        operator RuntimeState&() const
        {
            return *state;
        }

        std::unique_ptr<RuntimeState> state;
    };

    /// @brief
    /// Purpose: Runs one frame: presents the previous one, pumps OS events/jobs/events,
    /// updates streaming/scripts/fixed-step, and renders through the app's graph (unless
    /// graphics.is_custom_pipeline — then the host draws). The first call boots
    /// (CREATED -> RUNNING); false means the app stopped — teardown is the Runtime's
    /// destructor. THE loop: `while (tbx::run(runtime)) { ... }`.
    TBX_API bool run(Runtime& runtime);

    /// @brief
    /// Purpose: Requests a clean exit — the next run() stops the app.
    TBX_API void quit(Runtime& runtime);
}
