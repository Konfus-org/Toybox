#pragma once
#include "tbx/api.h"
#include "tbx/ui/document.h"
#include "tbx/utils/typedefs.h"

// The engine's debug overlay: a built-in feature bound to F3. The public verb is
// update_debugging(RuntimeState&) — tbx::run() calls it every frame; it unpacks the runtime
// and drives the overlay (toggle, first-open document load, per-frame stat refresh). The
// detailed worker (update_debug_view) is an internal impl detail in src/debug/view.h.
namespace tbx
{
    struct RuntimeState;

    /// @brief
    /// Purpose: The debug overlay's state, held by value on the Runtime: its document text,
    /// visibility, and smoothed timings.
    struct TBX_API DebugViewState
    {
        Document document = {};
        uint64 frame = 0;
        bool is_open = false;
        float smoothed_delta = 0.0f;
        float refresh_timer = 0.0f;
    };

    /// @brief
    /// Purpose: Runs the debug overlay for one frame from the whole runtime — F3 toggles it,
    /// the first open loads its document, and its stats land in the ui bindings. Called by
    /// tbx::run() every frame.
    TBX_API void update_debugging(RuntimeState& state);
}
