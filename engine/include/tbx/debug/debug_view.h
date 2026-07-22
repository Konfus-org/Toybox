#pragma once
#include "tbx/assets/assets.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/platform/input.h"
#include "tbx/platform/window.h"
#include "tbx/ui/ui.h"
#include "tbx/ui/ui_document.h"
#include "tbx/utils/api.h"
#include "tbx/utils/typedefs.h"
#include <functional>
#include <optional>

// The engine's debug overlay: a built-in feature bound to F3. One public verb — update() —
// owns everything: the toggle, loading its document on first open, and refreshing the stats
// it pushes into the ui bindings. The ui pass composites the state's document whenever it is
// open and loaded.
namespace tbx::debug::view
{
    /// @brief
    /// Purpose: The debug overlay's state, held by value on the Runtime: its document text,
    /// visibility, and smoothed timings. update() loads the document on first open.
    struct TBX_API DebugState
    {
        ui::UiDocument document = {};
        uint64 frame = 0;
        bool is_open = false;
        float smoothed_delta = 0.0f;
        float refresh_timer = 0.0f;
    };

    /// @brief
    /// Purpose: Runs the overlay for one frame — F3 toggles it, the first open loads its
    /// document, and its stats land in the runtime's ui bindings. Called by tbx::run()
    /// every frame.
    TBX_API void update(
        DebugState& state,
        const input::InputState& input,
        const ecs::Sandbox& sandbox,
        const assets::AssetsState& assets,
        const windows::WindowsState& windows,
        ui::UiState& ui,
        float delta_time);
}
