#pragma once
#include "tbx/platform/input.h"

// Internal input machinery — kept out of the public input.h. The state-taking queries stay public
// (they are the testable input API); only the engine-driven per-frame pump lives here.
namespace tbx::internal
{
    /// @brief
    /// Purpose: The per-frame input pass: rolls frame state, then pumps OS input events
    /// (keyboard, mouse, controllers) into the input state, emitting key transitions through
    /// events. Implemented by the platform backend; headless runs roll but skip the pump.
    void update_input(InputState& input, EventsState& events);
}
