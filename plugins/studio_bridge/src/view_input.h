#pragma once
#include "tbx/types/typedefs.h"
#include <vector>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: The mouse-button bits of ViewInput::buttons, as the editor packs them into the
    /// view.input notification.
    namespace ViewButtons
    {
        inline constexpr uint32 LEFT = 0x1U;
        inline constexpr uint32 RIGHT = 0x2U;
        inline constexpr uint32 MIDDLE = 0x4U;
    }

    /// @brief
    /// Purpose: The forwarded viewport input for one view — the editor's view.input notification
    /// payload. Consumed by the input ops (editor fly + asset-preview orbit cameras, and the game
    /// input system) and the gizmo ops (hover/drag). Kept beside the views in the ViewState
    /// rather than on the ViewStream so a view stream stays purely render state.
    /// @details
    /// Ownership: Owned by the ViewState — one per live view, keyed by view name, created and dropped
    /// with the view. Thread Safety: Written (apply_view_input) and read (the on_update consumers) on
    /// the main thread under the ViewState lock.
    struct ViewInput
    {
        // Whether this view currently has the editor's input focus.
        bool focused = false;
        uint32 buttons = 0U; // ViewButtons bit set (LEFT / RIGHT / MIDDLE)
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

        // The view's held keys as tbx::InputKey codes, plus the absolute mouse position within the
        // view. Sent for every view kind: the game view feeds them into the engine input system while
        // playing; editor views read them for tool modifiers (the gizmo's snap-hold keys).
        std::vector<int> keys = {};
        float mouse_x = 0.0F;
        float mouse_y = 0.0F;
    };
}
