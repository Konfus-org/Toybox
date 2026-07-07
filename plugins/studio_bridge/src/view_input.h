#pragma once
#include "tbx/types/typedefs.h"
#include <vector>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: The forwarded viewport input for one view — the editor's view.input notification
    /// payload. Consumed by the InputController (editor fly + asset-preview orbit cameras, and the game
    /// input system) and the GizmoController (hover/drag). Kept beside the views in the ViewManager
    /// rather than on the ViewStream so a view stream stays purely render state.
    /// @details
    /// Ownership: Owned by the ViewManager — one per live view, keyed by view name, created and dropped
    /// with the view. Thread Safety: Written (apply_view_input) and read (the on_update consumers) on
    /// the main thread under the view-manager lock.
    struct ViewInput
    {
        // Whether this view currently has the editor's input focus.
        bool focused = false;
        uint32 buttons = 0U; // bit0 = left, bit1 = right, bit2 = middle
        // Mouse + wheel deltas accumulate between engine frames and are consumed when a view's camera
        // (or the game input system) applies them.
        float accumulated_mouse_dx = 0.0F;
        float accumulated_mouse_dy = 0.0F;
        float accumulated_wheel = 0.0F;
        // Normalized cursor in the rendered image (top-left origin), read by the gizmo.
        float cursor_u = 0.0F;
        float cursor_v = 0.0F;

        // Editor fly camera: pressed move-key bitset (bit0 fwd, 1 back, 2 left, 3 right, 4 up, 5 down).
        // Zero for the other view kinds (the editor only sends it for an editor view).
        uint32 move_keys = 0U;

        // Game view raw input fed into the engine input system while playing: pressed tbx::InputKey
        // codes plus the absolute mouse position within the view. Empty/zero for the other view kinds.
        std::vector<int> keys = {};
        float mouse_x = 0.0F;
        float mouse_y = 0.0F;
    };
}
