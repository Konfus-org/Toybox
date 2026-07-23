#pragma once
#include "tbx/assets/assets.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/events/events.h"
#include "tbx/scripting/scripts.h"

namespace tbx::internal
{
    struct RuntimeState;

    /// @brief
    /// Purpose: Builds the compiled-in scripting backends against this runtime. Idempotent.
    void initialize_scripting(RuntimeState& runtime);

    /// @brief
    /// Purpose: Runs every scripted toy's fixed-cadence hook; called from the fixed step alongside
    /// physics so scripts can do physics-rate work.
    void fixed_update_scripts(ScriptsState& state, Sandbox& sandbox, float fixed_delta_time);

    /// @brief
    /// Purpose: Runs every scripted toy across every backend, loading script-source assets and
    /// reaping scripts whose toy vanished. Called by tbx::run() every frame.
    void update_scripts(
        ScriptsState& state,
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        float delta_time);

    /// @brief
    /// Purpose: Fires cleanup then frees every live script instance — the shutdown pass, run once
    /// while the sandbox is still alive before the VMs are destroyed.
    void purge_scripts(ScriptsState& state, Sandbox& sandbox);
}
