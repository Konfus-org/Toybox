#pragma once
#include "tbx/api.h"
#include "tbx/app.h"
#include "tbx/assets/handle.h"
#include "tbx/utils/command_list.h"
#include <memory>

namespace tbx
{
    namespace internal
    {
        // The engine's whole state. Defined in src/runtime_state.h so it never reaches a public
        // header — external code uses the free-function API, not the state (see CodeStandards.md).
        struct RuntimeState;
    }

    /// @brief
    /// Purpose: Move-only value handle over stable state — moves are pointer swaps, so the
    /// functional dialect (state-advancing verbs consume and return the runtime) costs nothing and
    /// never invalidates async captures of the state. The state itself is an internal pImpl
    /// (internal::RuntimeState); external code never names it — it uses the free-function API
    /// (tbx::spawn, tbx::is_key_down, tbx::run, …). One Runtime with a window or UI at a time: the
    /// platform and UI libraries underneath are process-global.
    struct TBX_DLL_EXPORT Runtime
    {
        Runtime();
        explicit Runtime(App app);

        /// @brief
        /// Purpose: Boots from the app's .tapp asset — THE file-based entry: pass a handle to the
        /// .tapp (its folder becomes the asset root) plus the parsed command line, and run() loads
        /// and applies the config on the first frame. No manual init/deserialize — run() owns it.
        explicit Runtime(AssetHandle<App> app, CommandList commands = {});
        ~Runtime();

        Runtime(Runtime&&) noexcept;
        Runtime& operator=(Runtime&&) noexcept;

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
        /// Purpose: The one running runtime, published by run() for the frame. The deliberate single
        /// global that lets the script-facing engine API be plain free functions instead of threading
        /// state. MAIN THREAD ONLY — async work must capture the state it was handed, never call this.
        /// Callers touching members must include src/runtime_state.h (the full definition).
        TBX_DLL_EXPORT RuntimeState& get_runtime();

        /// @brief
        /// Purpose: True while a runtime is published (between the first run() and shutdown) — guard
        /// for convenience helpers that may run before boot or after teardown.
        TBX_DLL_EXPORT bool has_runtime();
    }
}
