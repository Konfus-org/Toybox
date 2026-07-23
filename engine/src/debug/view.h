#pragma once
#include "tbx/debug/debugging.h" // DebuggingState + the public update_debugging facade
#include "tbx/assets/assets.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/platform/input.h"
#include "tbx/platform/window.h"
#include "tbx/ui/ui.h"

// Internal impl header for the debug overlay (not on the public include path). update_debugging
// (debugging.cpp) unpacks the runtime and calls update_debug_view below, which does the work:
// F3 toggle, first-open document load, and per-frame stat refresh into the ui bindings.
namespace tbx
{
    /// @brief
    /// Purpose: Runs the overlay for one frame from its unpacked pieces — F3 toggles it, the
    /// first open loads its document, and its stats land in the runtime's ui bindings.
    TBX_DLL_EXPORT void update_debug_view(
        DebuggingState& state,
        const InputState& input,
        const Sandbox& sandbox,
        const AssetsState& assets,
        const WindowsState& windows,
        UiState& ui,
        float delta_time);
}
