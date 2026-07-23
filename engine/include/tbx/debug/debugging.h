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

    /// @brief
    /// Purpose: The debug overlay's state, held by value on the Runtime: its document text,
    /// visibility, and smoothed timings.
    struct TBX_DLL_EXPORT DebuggingState
    {
        Document document = {};
        uint64 frame = 0;
        bool is_open = false;
        float smoothed_delta = 0.0f;
        float refresh_timer = 0.0f;
    };

}
